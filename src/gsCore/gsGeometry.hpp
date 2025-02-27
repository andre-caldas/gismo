/** @file gsGeometry.hpp

    @brief Provides implementation of Geometry default operatiions.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): A. Mantzaflaris
*/

#pragma once

#include <gsCore/gsBasis.h>
#include <gsUtils/gsMesh/gsMesh.h>
#include <gsCore/gsFuncData.h>
#include <gsCore/gsFuncCoordinate.h>

#include <gsCore/gsGeometrySlice.h>

//#include <gsCore/gsMinimizer.h>
#include <gsUtils/gsPointGrid.h>

namespace gismo
{

/// Squared distance function from a fixed point to a gsGeometry
template<class T>
class gsSquaredDistance GISMO_FINAL : public gsFunction<T>
{
public:
    gsSquaredDistance(const gsGeometry<T> & g, const gsVector<T> & pt)
        : m_g(&g), m_pt(&pt), m_gd(2) { }

    // f  = (1/2)*||x-pt||^2
    void eval_into(const gsMatrix<T>& u, gsMatrix<T>& result) const
    {
        m_g->eval_into(u, m_gd[0]);
        result.resize(1, u.cols());
        result.at(0) = 0.5 * (m_gd[0]-*m_pt).squaredNorm();
    }

    void evalAllDers_into(const gsMatrix<T> & u, int n,
                          std::vector<gsMatrix<T> > & result,
                          bool sameElement = false) const
    {
        GISMO_ASSERT(1==u.cols(), "Single argument assumed");
        result.resize(n+1);
        m_g->evalAllDers_into(u, n, m_gd, sameElement);

        // f  = (1/2)*||x-pt||^2
        result[0].resize(1, 1);
        result[0].at(0) = 0.5 * (m_gd[0]-*m_pt).squaredNorm();
        if (n==0) return;

        // f' = x'*(x-pt)
        auto jacT = m_gd[1].reshaped(u.rows(),m_pt->rows());
        result[1].noalias() = jacT * (m_gd[0] - *m_pt);
        if (n==1) return;

        // f'' = tr(x')*x' + sum_i[ (x_i-pt_i) * x_i'']
        tmp.noalias() = jacT * jacT.transpose();
        index_t d2  = u.rows() * (u.rows()+1) / 2;
        gsMatrix<T> hm;
        for ( index_t k=0; k < m_g->coefs().cols(); ++k )
        {
            hm = util::secDerToHessian(m_gd[2].block(k*d2,0,d2,1),u.rows()).reshaped(u.rows(),u.rows());
            tmp += (m_gd[0].at(k)-m_pt->at(k)) * hm;
        }
        util::hessianToSecDer(tmp,u.rows(),result[2]);
    }

    // f' = x'*(x-pt)
    void deriv_into(const gsMatrix<T>& u, gsMatrix<T>& result) const
    {
        result.resize(u.rows(), u.cols());
        for ( index_t i=0; i != u.cols(); i++ )
        {
            tmp = u.col(i);
            m_g->eval_into(tmp,m_gd[0]);
            m_g->jacobian_into(tmp,m_gd[1]);
            result.col(i).noalias() = m_gd[1].transpose() * (m_gd[0] - *m_pt);
        }
    }

    // f'' = tr(x')*x' + sum_i[ (x_i-pt_i) * x_i'']
    void hessian_into(const gsMatrix<T>& u, gsMatrix<T>& result,
                      index_t) const
    {
        m_g->eval_into(u,m_gd[0]);
        m_g->jacobian_into(u,m_gd[1]);
        result.noalias() = m_gd[1].transpose() * m_gd[1];
        for ( index_t k=0; k < m_g->coefs().cols(); ++k )
        {
            tmp = m_g->hessian(u,k);
            result.noalias() += (m_gd[0].at(k)-m_pt->at(k))*tmp;
        }
    }

    gsMatrix<T> support() const {return m_g->support()  ;}
    short_t domainDim ()  const {return m_g->domainDim();}
    short_t targetDim ()  const {return 1;}

private:
    const gsGeometry<T> * m_g;
    const gsVector<T> * m_pt;
    mutable std::vector<gsMatrix<T> > m_gd;
    mutable gsMatrix<T> tmp;
};

    /*
template<class T>
boxSide gsGeometry<T>::sideOf( const gsVector<T> & u,  )
{
    // get the indices of the coefficients which lie on the boundary
    gsMatrix<index_t > allBnd = m_basis->allBoundary();
    gsMatrix<T> bndCoeff(allBnd.rows(), m_coefs.rows());

    // extract the indices of the boundary coefficients
    for(index_t r = 0; r < allBnd.rows(); r++)
        bndCoeff.row(r) = m_coefs.row(allBnd(r,0));


    for(size_t size = 0; size < allBnd.rows(); size++)
        if(boundaryPatch1[size] == 1)
            interfaceIndicesPatch1.push_back(allBnd(size, 0)); // get the indices of the coefficients on patch 1 which lie on the common interface

    boxSide side;

    for(unsigned index = 1; index <= nBoundaries; index++) {
        int contained = 0;
        side.m_index = index;

        gsMatrix<index_t> bnd = m_basis->boundary(side);

        for(size_t i = 0; i < interfaceIndicesPatch1.size(); i++)
        {
            gsInfo << "index: " << interfaceIndicesPatch1[i] << "\n";
            for (int j = 0; j < bnd.rows(); j++)
            {
                if(bnd(j, 0) == interfaceIndicesPatch1[i])
                    contained++;
            }
        }

        if(contained == bnd.rows())
            break;

        //gsInfo << "indices of boundary : " << bnd << "\n";
    }
}
     */

template<class T>
gsGeometry<T>::gsGeometry(const gsGeometry<T> & o)
: m_coefs(o.m_coefs), m_basis(o.m_basis != NULL ? o.basis().clone().release() : NULL), m_id(o.m_id)
{ }

template<class T>
gsGeometry<T>::~gsGeometry()
{ delete m_basis; }

template<class T>
void gsGeometry<T>::eval_into(const gsMatrix<T>& u, gsMatrix<T>& result) const
{ this->basis().evalFunc_into(u, m_coefs, result); }

template<class T>
void gsGeometry<T>::deriv_into(const gsMatrix<T>& u, gsMatrix<T>& result) const
{ this->basis().derivFunc_into(u, m_coefs, result); }

template<class T>
void gsGeometry<T>::deriv2_into(const gsMatrix<T>& u, gsMatrix<T>& result) const
{ this->basis().deriv2Func_into(u, m_coefs, result); }

template<class T>
void gsGeometry<T>::evalAllDers_into(const gsMatrix<T> & u, int n,
                                     std::vector<gsMatrix<T> > & result,
                                     bool sameElement) const
{ this->basis().evalAllDersFunc_into(u, m_coefs, n, result, sameElement); }

template<class T>
short_t gsGeometry<T>::domainDim() const { return this->basis().domainDim(); }

template<class T>
short_t gsGeometry<T>::coDim() const { return coefDim()-this->basis().domainDim(); }

template<class T>
short_t gsGeometry<T>::parDim() const { return this->basis().domainDim(); }

template<class T>
gsMatrix<T> gsGeometry<T>::support() const
{ return this->basis().support(); }

template<class T>
/// Returns the range of parameters as a matrix with two columns, [lower upper]
gsMatrix<T> gsGeometry<T>::parameterRange() const
{ return this->basis().support(); }

template<class T>
gsGeometry<T>& gsGeometry<T>::operator=( const gsGeometry<T> & o)
{
    if ( this != &o )
    {
        m_coefs = o.m_coefs;
        delete m_basis;
        m_basis = o.basis().clone().release() ;
        m_id = o.m_id;
    }
    return *this;
}

template<class T>
typename gsGeometry<T>::uPtr
gsGeometry<T>::boundary(boxSide const& s) const
{
    gsMatrix<index_t> ind = this->basis().boundary(s); // get indices of the boundary DOF
    gsMatrix<T> coeffs (ind.size(), geoDim()); // create matrix for boundary coefficients

    for (index_t i=0; i != ind.size(); i++ )
    {
        coeffs.row(i) = m_coefs.row( ind(i,0) );
    }

    typename gsBasis<T>::uPtr Bs = this->basis().boundaryBasis(s);  // Basis for boundary side s
    return Bs->makeGeometry( give(coeffs) );
}

template<class T>
typename gsGeometry<T>::uPtr
gsGeometry<T>::iface(const boundaryInterface &,
                     const gsGeometry &) const
{
    GISMO_NO_IMPLEMENTATION
}

template<class T>
typename gsGeometry<T>::uPtr
gsGeometry<T>::component(boxComponent const& bc) const
{
    gsMatrix<index_t> ind;
    typename gsBasis<T>::uPtr Bs = this->basis().componentBasis_withIndices(bc, ind, false);
    gsMatrix<T> coefs (ind.size(), geoDim());

    for (index_t i=0; i != ind.size(); i++ )
    {
        coefs.row(i) = m_coefs.row( ind(i,0) );
    }

    return Bs->makeGeometry( coefs );
}

template<class T>
void gsGeometry<T>::evaluateMesh(gsMesh<T>& mesh) const
{
    const int pDim = parDim();
    const int gDim = geoDim();

    gsMatrix<T> tmp;

    // For all vertices of the mesh, push forward the value by the
    // geometry mapping
    if (1==gDim && 3>pDim) // Plot a graph
        for (size_t i = 0; i!= mesh.numVertices(); ++i)
        {
            eval_into( mesh.vertex(i).topRows(pDim), tmp );
            mesh.vertex(i).middleRows(pDim, gDim) = tmp;
        }
    else // Plot mesh on a mapping
        for (size_t i = 0; i!= mesh.numVertices(); ++i)
        {
            eval_into( mesh.vertex(i).topRows(pDim), tmp );
            const index_t gd = math::min(3,gDim);
            mesh.vertex(i).topRows(gd) = tmp.topRows(gd);
        }

}
template<class T>
std::vector<gsGeometry<T>* > gsGeometry<T>::uniformSplit(index_t) const
{
    GISMO_NO_IMPLEMENTATION
}

template<class T>
gsGeometrySlice<T> gsGeometry<T>::getIsoParametricSlice(index_t dir_fixed, T par) const
{
    return gsGeometrySlice<T>(this,dir_fixed,par);
}

template<class T>
typename gsMatrix<T>::RowXpr
gsGeometry<T>::coefAtCorner(boxCorner const & c)
{
    return this->m_coefs.row(this->basis().functionAtCorner(c));
}

template<class T>
typename gsMatrix<T>::ConstRowXpr
gsGeometry<T>::coefAtCorner(boxCorner const & c) const
{
    return this->m_coefs.row(this->basis().functionAtCorner(c));
}

template<class T>
void gsGeometry<T>::uniformRefine(int numKnots, int mul, short_t const dir)
{
    this->basis().uniformRefine_withCoefs( m_coefs, numKnots, mul, dir);
}

template<class T>
void gsGeometry<T>::uniformCoarsen(int numKnots) // todo: int dir = -1
{
    this->basis().uniformCoarsen_withCoefs( m_coefs, numKnots);
}

template<class T>
void gsGeometry<T>::refineElements( std::vector<index_t> const & boxes )
{
    this->basis().refineElements_withCoefs(this->m_coefs, boxes );
}

template<class T>
void gsGeometry<T>::unrefineElements( std::vector<index_t> const & boxes )
{
    this->basis().unrefineElements_withCoefs(this->m_coefs, boxes );
}

template<class T>
inline typename gsGeometry<T>::uPtr gsGeometry<T>::coord(const index_t c) const {return this->basis().makeGeometry( this->coefs().col(c) ); }

template<class T>
short_t gsGeometry<T>::degree(const short_t & i) const
    //{ return this->basisComponent(i).degree(); };
    { return this->basis().degree(i); }


template<class T>
T gsGeometry<T>::closestPointTo(const gsVector<T> & pt,
                                gsVector<T> & result,
                                const T accuracy,
                                const bool useInitialPoint) const
{
    GISMO_ASSERT( pt.rows() == targetDim(), "Invalid input point." <<
                  pt.rows() <<"!="<< targetDim() );
#if false
    gsSquaredDistance<T> dist2(*this, pt);
    gsMinimizer<T> fmin(dist2);
    fmin.solve();
    result = fmin.currentDesign();
#else
    gsSquaredDistance<T> dist2(*this, pt);
    result = useInitialPoint ? dist2.argMin(accuracy*accuracy, 100, result)
    : dist2.argMin(accuracy*accuracy, 100) ;
#endif
    return math::sqrt( dist2.eval(result).value() );
}

template<class T>
T gsGeometry<T>::directedHausdorffDistance(const gsGeometry & other, const index_t nsamples, const T accuracy) const
{
    // Sample points on *this
    gsMatrix<T> uv = gsPointGrid<T>(this->support(),nsamples);
    gsMatrix<T> pts;
    this->eval_into(uv,pts);
    // Find the maximum of the closest point on *other from the set of pts
    T maxDist=std::numeric_limits<T>::min();
    gsVector<T> tmp;
    for (index_t k=0; k!=pts.cols(); k++)
    {
        maxDist = std::max(maxDist,other.closestPointTo(pts.col(k),tmp,accuracy,false));
    }
    return math::sqrt(2*maxDist); // euclidean distance since closestPointTo uses 1/2*||x-y||^2, see gsSquaredDistance
}

template<class T>
T gsGeometry<T>::HausdorffDistance(const gsGeometry & other, const index_t nsamples, const T accuracy, bool directed) const
{
    T this2other, other2this;
    this2other = this->directedHausdorffDistance(other,nsamples,accuracy);
    if (directed)
        return this2other;
    else
    {
        other2this = other.directedHausdorffDistance(*this,nsamples,accuracy);
        return std::max(other2this,this2other);
    }
}


// Recovers the points of the geometry from the given points and parameters
template<class T>
void gsGeometry<T>::recoverPoints(gsMatrix<T> & xyz, gsMatrix<T> & uv, index_t k,
                                  const T accuracy) const
{
    gsVector<index_t> ind(xyz.rows()-1);
    for (index_t i = 0; i!= xyz.rows(); ++i)
        if (i<k) ind[i]=i;
        else if (i>k) ind[i-1]=i;

    gsMatrix<T> pt = xyz(ind,gsEigen::all);
    gsFuncCoordinate<T> fc(*this, give(ind));
    fc.invertPoints(pt,uv,accuracy,false);
    xyz = this->eval(uv);
    //possible check: pt close to xyz
}


template<class T>
std::ostream & gsGeometry<T>::print(std::ostream &os) const
{
    os << "Geometry "<< "R^"<< this->parDim() <<
        " --> R^"<< this->geoDim()<< ", #control pnts= "<< coefsSize() <<
        ": "<< coef(0) <<" ... "<< coef(this->coefsSize()-1);
    os<<"\nBasis:\n" << this->basis();
    return os;
}

template<class T>
void gsGeometry<T>::merge(gsGeometry *)
{ GISMO_NO_IMPLEMENTATION }

template<class T>
void gsGeometry<T>::toMesh(gsMesh<T> &, int) const
{ GISMO_NO_IMPLEMENTATION }

template<class T>
typename gsGeometry<T>::uPtr
gsGeometry<T>::approximateLinearly() const
{ GISMO_NO_IMPLEMENTATION }

template<class T>
void gsGeometry<T>::outerNormal_into(const gsMatrix<T>&, gsMatrix<T> &) const
{ GISMO_NO_IMPLEMENTATION }

template<class T>
std::vector<gsGeometry<T> *> gsGeometry<T>::boundary() const
{
    // TO DO: get boundary curves, using basis().boundary();
    GISMO_NO_IMPLEMENTATION
}

template<class T>
void gsGeometry<T>::insertKnot( T, index_t, index_t)
{
    GISMO_NO_IMPLEMENTATION
}

template<class T>
void gsGeometry<T>::degreeElevate(short_t const i, short_t const dir)
{
    typename gsBasis<T>::uPtr b = m_basis->clone();
    b->degreeElevate(i, dir);
    gsMatrix<T> iVals, iPts = b->anchors();
    this->eval_into(iPts, iVals);
    typename gsGeometry<T>::uPtr g = b->interpolateData(iVals, iPts);

    std::swap(m_basis, g->m_basis);
    g->coefs().swap(this->coefs());
}

template<class T>
void gsGeometry<T>::degreeReduce(short_t const i, short_t const dir)
{
    typename gsBasis<T>::uPtr b = m_basis->clone();
    b->degreeReduce(i, dir);
    gsMatrix<T> iVals, iPts = b->anchors();
    this->eval_into(iPts, iVals);
    typename gsGeometry<T>::uPtr g = b->interpolateData(iVals, iPts);

    std::swap(m_basis, g->m_basis);
    g->coefs().swap(this->coefs());
}

template<class T>
void gsGeometry<T>::degreeIncrease(short_t const i, short_t const dir)
{
    typename gsBasis<T>::uPtr b = m_basis->clone();
    b->degreeIncrease(i, dir);
    gsMatrix<T> iVals, iPts = b->anchors();
    this->eval_into(iPts, iVals);
    typename gsGeometry<T>::uPtr g = b->interpolateData(iVals, iPts);

    std::swap(m_basis, g->m_basis);
    g->coefs().swap(this->coefs());
}

template<class T>
void gsGeometry<T>::degreeDecrease(short_t const i, short_t const dir)
{
    typename gsBasis<T>::uPtr b = m_basis->clone();
    b->degreeDecrease(i, dir);
    gsMatrix<T> iVals, iPts = b->anchors();
    this->eval_into(iPts, iVals);
    typename gsGeometry<T>::uPtr g = b->interpolateData(iVals, iPts);

    std::swap(m_basis, g->m_basis);
    g->coefs().swap(this->coefs());
}

template<class T> void
gsGeometry<T>::hessian_into(const gsMatrix<T>& u, gsMatrix<T> & result,
                            index_t coord) const
{
    const unsigned d = this->domainDim();
    GISMO_ASSERT( coord<targetDim(),"Invalid coordinate function "<<coord);

    gsMatrix<T> B, tmp(d,d);
    gsMatrix<index_t> ind;

    // coefficient matrix row k = coef. of basis function k
    const gsMatrix<T>& C = this->m_coefs;
    // col j = nonzero second derivatives at column point u(..,j)
    m_basis->deriv2_into(u, B) ;
    // col j = indices of active functions at column point u(..,j)
    m_basis->active_into(u, ind);

    result.setZero(d,d);
    static const index_t j = 0;// just one column
    //for ( unsigned j=0; j< u.cols(); j++ ) // for all points (columns of u)

    for ( index_t i=0; i< ind.rows() ; i++ ) // for all non-zero basis functions)
    {
        unsigned r = i*d*(d+1)/2;
        unsigned m = d;
        //construct the Hessian of basis function ind(i,0)
        for (unsigned k=0; k<d; ++k ) // for all rows
        {
            tmp(k,k) = B(r+k,j);
            for (unsigned l=k+1; l<d; ++l ) // for all cols
                tmp(k,l) = tmp(l,k) = B(r + m++,0);
        }
        result += C(ind(i,j), coord) * tmp;
    }
}

template<class T>
void gsGeometry<T>::controlNet( gsMesh<T> & mesh) const
{ basis().connectivity(m_coefs, mesh); }



template <typename T>
void extractRows( const gsMatrix<T> &in, typename gsMatrix<index_t>::constColumn actives, gsMatrix<T> &out)
{
    out.resize(actives.rows(), in.cols());
    for (index_t r=0; r<actives.rows();++r)
        out.row(r)=in.row(actives(r,0));
}

template <class T>
void
gsGeometry<T>::compute(const gsMatrix<T> & in, gsFuncData<T> & out) const
{
    const unsigned flags = out.flags | NEED_ACTIVE;
    const index_t  numPt = in.cols();
    const index_t  numCo = m_coefs.cols();

    gsFuncData<T> tmp(flags);
    this->basis().compute(in, tmp);

    out.values.resize(out.maxDeriv()+1);
    out.dim.first  = tmp.dim.first;
    out.dim.second = numCo;
    if ( flags & SAME_ELEMENT )
    {
        gsMatrix<T> coefM;
        extractRows(m_coefs,tmp.active(0),coefM);

        if (flags & NEED_VALUE)
            out.values[0].noalias()=coefM.transpose()*tmp.values[0];
        if (flags & NEED_DERIV)
        {
            const index_t derS = tmp.derivSize();
            out.values[1].resize(derS*numCo,numPt);
            for (index_t p=0; p< numPt; ++p)
                out.values[1].reshapeCol(p, derS, numCo).noalias() = tmp.deriv(p)*coefM;
        }
        if (flags & NEED_DERIV2)
        {
            const index_t derS = tmp.deriv2Size();
            out.values[2].resize(derS*numCo,numPt);
            for (index_t p=0; p< numPt; ++p)
                out.values[2].reshapeCol(p, derS, numCo).noalias() = tmp.deriv2(p)*coefM;
        }
        if (flags & NEED_ACTIVE)
            this->active_into(in.col(0), out.actives);
    }
    else
    {
        gsMatrix<T> coefM;
        const index_t derS = tmp.derivSize();
        const index_t der2S = tmp.deriv2Size();

        if (flags & NEED_VALUE)  out.values[0].resize(numCo,numPt);
        if (flags & NEED_DERIV)  out.values[1].resize(numCo*derS,numPt);
        if (flags & NEED_DERIV2) out.values[2].resize(numCo*der2S,numPt);
        if (flags & NEED_ACTIVE) this->active_into(in, out.actives);

        for (index_t p=0; p<numPt;++p)
        {
            extractRows(m_coefs,tmp.active(p),coefM);
            if (flags & NEED_VALUE)
                out.values[0].reshapeCol(p,1,numCo).noalias() = tmp.eval(p)*coefM;
            if (flags & NEED_DERIV)
                out.values[1].reshapeCol(p, derS, numCo).noalias() = tmp.deriv(p)*coefM;
            if (flags & NEED_DERIV2)
                out.values[2].reshapeCol(p, der2S, numCo).noalias() = tmp.deriv2(p)*coefM;
        }
    }
}

template<class T>
void gsGeometry<T>::toggleOrientation()
{
    GISMO_NO_IMPLEMENTATION
}

template<class T>
std::vector<boxSide> gsGeometry<T>::locateOn(const gsMatrix<T> & u, gsVector<bool> & onGeo, gsMatrix<T> & preIm, bool lookForBoundary, real_t tol) const
{
    onGeo.resize(u.cols());
    std::vector<boxSide> sides(u.cols());

    for(index_t i = 0; i < onGeo.size(); i++)
        onGeo(i) = false;

    preIm.resize(geoDim(), u.cols());
    gsMatrix<T> pr = this->parameterRange(), tmp;

    for(index_t i = 0; i < u.cols(); i++)
    {
        this->invertPoints(u.col(i), tmp, tol);
        pr = this->parameterRange();
        //if ((tmp.array() >= pr.col(0).array()).all()
        //    && (tmp.array() <= pr.col(1).array()).all())
        if ((tmp.array() >= pr.col(0).array() - 1.e-4).all()
             && (tmp.array() <= pr.col(1).array() + 1.e-4).all()) // be careful! if u is on the boundary then we may get a wrong result
            // the tolerance is due to imprecisions in the geometry map. E.g. If a circle is rotated then the corner need
            // not to lie exactly on the interface of the neighbour patch since we use only B-splines for the modelling
            // TODO: Maybe find a better solution!
        {
            onGeo(i) = true;
            preIm.col(i) = tmp;

            if (lookForBoundary == true)
            {
                boxSide s;
                for (int d = 0; d < geoDim(); d++) {
                    if ((math::abs(tmp(d, 0) - pr(d, 0)) < tol))
                    {
                        s.m_index = 2*d+1; // lower
                        break;
                    }

                    if ((math::abs(tmp(d, 0) - pr(d, 1)) < tol))
                    {
                        s.m_index = 2 * d + 2; // upper
                        break;
                    }
                }
                sides[i] = s;
            }
        }
    }

    return sides;
}


} // namespace gismo
