/** @file gsHTensorBasis.hpp

    @brief Provides implementation of HTensorBasis common operations.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): G. Kiss, A. Mantzaflaris, J. Speh
*/

#pragma once

#include <gsUtils/gsMesh/gsMesh.h>

#include <gsUtils/gsPointGrid.h>

#include <gsIO/gsXml.h>
#include <gsIO/gsXmlGenericUtils.hpp>

namespace gismo
{
 // Central element implementation
// template<short_t d, class T>
// gsMatrix<T> gsHTensorBasis<d,T>::elementInSupportOf(index_t i) const
// {
//     index_t lvl = levelOf(i);
//     index_t j = flatTensorIndexOf(i);
//     return m_bases[lvl]->elementInSupportOf(j);
// }

template<short_t d, class T>
void gsHTensorBasis<d,T>::addLevel( const gsTensorBSplineBasis<d, T>& next_basis)
{
    GISMO_ENSURE(m_manualLevels,"Add level only works when m_manualLevels==true");
    // Fill the unique indices
    std::vector<std::vector<index_t>> lvlIndices(d);
    const tensorBasis * tb2 = dynamic_cast<const tensorBasis*>(m_bases.back());
    std::vector<T> difference, intersection;
    std::vector<T> knots1, knots2;
    std::vector<index_t> dirIndices;
    gsKnotVector<T> tmpknots;
    for (short_t dim=0; dim!=d; dim++)
    {
        dirIndices = m_uIndices.back()[dim];
        // Take the difference of the knot vectors
        tmpknots= tb2->knots(dim);

        GISMO_ASSERT(tmpknots.maxInteriorMultiplicity()<tmpknots.degree()+1,"Knot vector cannot have knot multiplicities higher than p="<<tmpknots.degree());

        knots1  = tb2->knots(dim).unique();
        knots2  = next_basis.knots(dim).unique();

        // Check nestedness.
        intersection.clear();
        difference.clear();
        // The unique knots of the new basis must contain the ones of the previous level
        std::set_intersection(  knots2.begin(), knots2.end(),
                                knots1.begin(), knots1.end(),
                                std::back_inserter(intersection) );
        // The difference of the two must not contain any knot in knots1!
        std::set_difference(  knots1.begin(), knots1.end(),
                                intersection.begin(), intersection.end(),
                                std::back_inserter(difference) );
        GISMO_ASSERT(difference.size()==0,"Knot vector is not nested!");

        difference.clear();
        // The difference of the two must not contain any knot in knots1!
        std::set_difference(knots2.begin(), knots2.end(),
                            knots1.begin(), knots1.end(),
                                std::back_inserter(difference) );

        // We reverse since later on we loop and pop the elements on the back
        std::reverse(difference.begin(),difference.end());
        // Double all indices in dirIndices. These correspond to the indices that come from the nested bases
        std::transform(dirIndices.begin(), dirIndices.end(), dirIndices.begin(), [=](index_t& i){return 2*i;});

        // Find the extra indices for the nodes in difference
        while (difference.size()!=0)
        {
            index_t i, n;
            typename std::vector<T>::iterator diff_ptr = difference.begin();
            // Find the index of the highest knot below the different knot
            i = tmpknots.uFind(*diff_ptr).uIndex(); // index of the knot span in which *it is located

            // Count how many difference knots lay in the same span
            for (n = 0, diff_ptr = difference.begin(); diff_ptr!=difference.end() && tmpknots.uFind(*diff_ptr).uIndex()==i; n++, diff_ptr++);
            // Count the distance between two element indices. It should be larger than n
            GISMO_ENSURE(n < dirIndices[i+1] - dirIndices[i],"Trying to insert more knots than available in the tensor structure");

            index_t newIdx;
            if (n % 2 != 0)
            {
                // We add the middle knot
                diff_ptr = std::next(difference.begin(),(n-1)/2);
                newIdx = (dirIndices[i] + dirIndices[i+1]) / 2;
            }
            else
            {
                // We add the first knot
                diff_ptr = difference.begin();
                newIdx = dirIndices[i] + 1;
            }

            // Add the correct index
            dirIndices.insert(std::upper_bound( dirIndices.begin(), dirIndices.end(), newIdx ),newIdx);
            // Insert the removed difference knot in the temporary knot vector
            tmpknots.insert(*diff_ptr);
            // Remove the difference knot, since it is treated
            difference.erase(diff_ptr);
        }

        lvlIndices[dim] = dirIndices;
    }
    m_uIndices.push_back(lvlIndices);

    // m_bases.push_back( new gsTensorBSplineBasis<d, T>( give( next_basis ) ));
    m_bases.push_back( next_basis.clone().release() );
}

template<short_t d, class T>
gsMatrix<T> gsHTensorBasis<d,T>::support() const
{
    return m_bases[0]->support();
}

//i in continuous numbering
template<short_t d, class T>
gsMatrix<T> gsHTensorBasis<d,T>::support(const index_t & i) const
{
    // Get the level
    int lvl = levelOf(i);
    // Return the the support
    return m_bases[lvl]->support( this->flatTensorIndexOf(i) );
}

// S.K.
template<short_t d, class T> inline
index_t gsHTensorBasis<d,T>::getLevelAtPoint(const gsMatrix<T> & Pt) const
{
    GISMO_ASSERT(Pt.cols() == 1, "Waiting for single point");
    point loIdx;

    const int maxLevel = m_tree.getMaxInsLevel();

    needLevel(maxLevel);

    for( int i =0; i < Dim; i++)
        loIdx[i] = m_bases[maxLevel]->knots(i).uFind( Pt(i,0) ).uIndex();

    if (m_manualLevels)
        this->_knotIndexToDiadicIndex(maxLevel,loIdx);

    return m_tree.levelOf( loIdx, maxLevel);
}

template<short_t d, class T> inline
index_t gsHTensorBasis<d,T>::getLevelAtIndex(const point & Pt) const
{
    const int maxLevel = m_tree.getMaxInsLevel();

    needLevel(maxLevel);

    return m_tree.levelOf( Pt, maxLevel);
}

template<short_t d, class T> inline
void gsHTensorBasis<d,T>::getLevelUniqueSpanAtPoints(const  gsMatrix<T> & Pt,
                                                     gsVector<index_t> & lvl,
                                                     gsMatrix<index_t> & loIdx ) const
{
    lvl.resize( Pt.cols() );
    loIdx.resize( Pt.rows(), Pt.cols() );
    lvl.setZero();
    loIdx.setZero();
    for( index_t i = 0; i < Pt.cols(); i++)
    {
        lvl[i] = getLevelAtPoint( Pt.col(i) );
        for( index_t j = 0; j < Pt.rows(); j++)
            loIdx(j,i) = m_bases[ lvl[i] ]->knots(j).uFind( Pt(j,i) ).uIndex() ;
    }
}

template<short_t d, class T> inline
void gsHTensorBasis<d,T>::numActive_into(const gsMatrix<T> & u, gsVector<index_t>& result) const
{
    result.resize( u.cols() );
    result.setZero();

    point low, upp, cur;
    const int maxLevel = m_tree.getMaxInsLevel();

    for(index_t p = 0; p < u.cols(); p++ ) //for all input points
    {
        for(short_t i = 0; i != d; ++i)
            low[i] = m_bases[maxLevel]->knots(i).uFind(u(i,p)).uIndex();

        // Identify the level of the point
        const int lvl = m_tree.levelOf(low, maxLevel);

        for(int i = 0; i <= lvl; i++)
        {
            m_bases[i]->active_cwise(u.col(p), low, upp);
            cur = low;
            do //iterate over all points in [low,upp]
            {
                CMatrix::const_iterator it =
                    m_xmatrix[i].find_it_or_fail( m_bases[i]->index(cur) );

                if( it != m_xmatrix[i].end() )// if index is found
                    result[p]++;
            }
            while( nextCubePoint(cur,low,upp) );
        }
    }
}

template<short_t d, class T>
void gsHTensorBasis<d, T>::addConnectivity(int lvl, gsMesh<T> & mesh) const
{
    const gsVector<index_t, d> & low = gsVector<index_t, d>::Zero();

    const gsTensorBSplineBasis<d, T> & bb = *m_bases[lvl];
    const CMatrix & cmat = m_xmatrix[lvl];

    // Last tensor-index in level lvl
    gsVector<index_t, d> end(d);
    for (index_t i = 0; i < d; ++i)
        end(i) = bb.component(i).size() - 1;

    index_t k, s;
    gsVector<index_t, d> v, upp;
    for (index_t i = 0; i < d; ++i) // For all axes
    {
        s = bb.stride(i);
        v = low;
        upp = end;
        upp[i] = 0; // suppress to face v[i]==0

        do // Insert all edges normal to axis i
        {
            k = bb.index(v);
            for (index_t j = 0; j != end[i]; ++j)
            {
                if (cmat.bContains(k) && cmat.bContains(k + s))
                {
                    // inefficient for now
                    const index_t kInd = m_xmatrix_offset[lvl] +
                        (std::lower_bound(cmat.begin(), cmat.end(), k)
                         - cmat.begin());

                    // inefficient for now
                    const index_t kNextInd = m_xmatrix_offset[lvl] +
                        (std::lower_bound(cmat.begin(), cmat.end(), k + s)
                         - cmat.begin());

                    mesh.addEdge(kInd, kNextInd);
                }
                k += s;
            }
        } while (nextCubePoint(v, low, upp));
    }
}

template<short_t d, class T>
void gsHTensorBasis<d, T>::connectivity(const gsMatrix<T> & nodes, int level, gsMesh<T> & mesh) const
{
    const index_t sz = size();
    GISMO_ASSERT(nodes.rows() == sz, "Invalid input.");

    // Add all vertices
    for (index_t i = 0; i< sz; ++i)
        mesh.addVertex(nodes.row(i).transpose());

    addConnectivity(level, mesh);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::connectivity(const gsMatrix<T> & nodes, gsMesh<T> & mesh) const
{
    const index_t sz  = size();
    GISMO_ASSERT( nodes.rows() == sz, "Invalid input.");

    // Add vertices
    for(index_t i = 0; i< sz; ++i )
        mesh.addVertex( nodes.row(i).transpose() );

    // For all levels
    for(size_t lvl = 0; lvl <= maxLevel(); lvl++)
    {
        addConnectivity(lvl, mesh);
    }
}

template<short_t d, class T>
index_t gsHTensorBasis<d,T>::size() const
{
    return m_xmatrix_offset.back();
}
template<short_t d, class T>
void gsHTensorBasis<d,T>::refine_withCoefs(gsMatrix<T> & coefs, gsMatrix<T> const & boxes)
{
    std::vector<gsSortedVector<index_t> > OX = m_xmatrix;
    refine(boxes);
    gsSparseMatrix<> transf;
    this->transfer(OX, transf);
    gsDebug<<"tranf orig:\n"<<transf<<std::endl;
    coefs = transf*coefs;
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::refineElements_withCoefs(gsMatrix<T> & coefs,std::vector<index_t> const & boxes)
{
    gsSparseMatrix<> transf;
    this->refineElements_withTransfer(boxes,transf);
    //gsDebug<<"tranf orig:\n"<<transf<<std::endl;
    coefs = transf*coefs;
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::refineElements_withTransfer(std::vector<index_t> const & boxes, gsSparseMatrix<T> & tran)
{
    std::vector<gsSortedVector<index_t> > OX = m_xmatrix;
    this->refineElements(boxes);
    this->transfer(OX, tran);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::refineElements_withTransfer2(std::vector<index_t> const & boxes, gsSparseMatrix<T> & tran)
{
    std::vector<gsSortedVector<index_t> > OX = m_xmatrix;
    this->refineElements(boxes);
    this->transfer2(OX, tran);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::refineElements_withCoefs2(gsMatrix<T> & coefs,std::vector<index_t> const & boxes)
{
    std::vector<gsSortedVector<index_t> > OX = m_xmatrix;
    refineElements(boxes);
    gsSparseMatrix<> transf;
    this->transfer2(OX, transf);
    //gsDebug<<"tranf 2:\n"<<transf<<std::endl;
    coefs = transf*coefs;
}

// template<short_t d, class T>
// void gsHTensorBasis<d,T>::unrefine_withCoefs(gsMatrix<T> & coefs, gsMatrix<T> const & boxes)
// {
//     std::vector<gsSortedVector<index_t> > OX = m_xmatrix;
//     unrefine(boxes);
//     gsSparseMatrix<> transf;
//     this->transfer(OX, transf);
//     gsDebug<<"tranf orig:\n"<<transf<<std::endl;
//     coefs = transf*coefs;
// }

template<short_t d, class T>
void gsHTensorBasis<d,T>::unrefineElements_withCoefs(gsMatrix<T> & coefs,std::vector<index_t> const & boxes)
{
    gsSparseMatrix<> transf;
    this->unrefineElements_withTransfer(boxes,transf);
    //gsDebug<<"tranf orig:\n"<<transf<<std::endl;

    typename gsSparseSolver<T>::QR solver(transf);
    coefs=solver.solve(coefs);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::unrefineElements_withTransfer(std::vector<index_t> const & boxes, gsSparseMatrix<T> & tran)
{
    typename gsHTensorBasis<d,T>::uPtr cp = this->clone();
    this->unrefineElements(boxes);
    cp->transfer(this->m_xmatrix,tran);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::uniformRefine_withCoefs(gsMatrix<T>& coefs, int numKnots, int mul, int dir)
{
    GISMO_UNUSED(dir);
    GISMO_ASSERT(dir==-1,"Direction is not implemented");

    std::vector<gsSortedVector<index_t> > OX = m_xmatrix;
    //uniformRefine(numKnots);
    //gsMatrix<> transf;
    //CMatrix temp;
    //this->m_xmatrix.insert(this->m_xmatrix.begin(), temp);
    //gsVector<index_t> level;
    //gsMatrix<index_t> p1, p2;
    //this->m_tree.getBoxes(p1,p2,level);
    std::vector<index_t> boxes;
    index_t lvl;
    for ( typename hdomain_type::literator it = m_tree.beginLeafIterator(); it.good(); it.next() )
    {
        //        gsDebug <<" level : "<< it.level() <<"\n";
        //        gsDebug <<" lower : "<< it.lowerCorner() <<"\n";
        //        gsDebug <<" upper : "<< it.upperCorner() <<"\n";

        lvl = it.level() + 1;
        const point & l = it.lowerCorner();
        const point & u = it.upperCorner();

        boxes.push_back(lvl);
        for( short_t i = 0; i < d; i++)
            boxes.push_back( l(i) * 2);
        for( short_t i = 0; i < d; i++)
            boxes.push_back( u(i) * 2);
    }

    this->clone()->refineElements_withCoefs(coefs, boxes);
    this->uniformRefine(numKnots, mul);
    //this->m_xmatrix.erase(this->m_xmatrix.begin(),this->m_xmatrix.begin()+1);
    //coefs = transf*coefs;
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::uniformCoarsen_withCoefs(gsMatrix<T>& coefs, int numKnots)
{
    std::vector<gsSortedVector<index_t> > OX = m_xmatrix;
    std::vector<index_t> boxes;
    index_t lvl;
    for ( typename hdomain_type::literator it = m_tree.beginLeafIterator(); it.good(); it.next() )
    {
        if (it.level() == 0)
            continue;
        lvl = it.level() - 1;
        const point & l = it.lowerCorner();
        const point & u = it.upperCorner();

        boxes.push_back(lvl);
        for( short_t i = 0; i < d; i++)
            boxes.push_back( l(i) / 2);
        for( short_t i = 0; i < d; i++)
            boxes.push_back( u(i)/2 + (index_t)(u(i)%2!=0));
    }

    this->clone()->unrefineElements_withCoefs(coefs, boxes);
    this->uniformCoarsen(numKnots);
}


template<short_t d, class T>
void gsHTensorBasis<d,T>::refine(gsMatrix<T> const & boxes, int refExt)
{
    GISMO_ASSERT(boxes.rows() == d, "refine() needs d rows of boxes.");
    GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provide refine() with them.");

#ifndef NDEBUG
    gsMatrix<T> para = support();
    for(int i = 0; i < boxes.cols()/2; i++)
    {
        for( short_t j = 0; j < d; j++ )
        {
            GISMO_ASSERT( para(j,0) <= boxes(j, 2*i) ,
                          "In refine() the first corner is outside the computational domain.");
            GISMO_ASSERT( para(j,1) >= boxes(j, 2*i+1),
                          "In refine() the second corner is outside the computational domain." );
        }
    }
#endif

    if( refExt == 0 )
    {
        // If there is no refinement-extension, just use the
        // "regular" refinement function refine( gsMatrix )
        this->refine( boxes );

        // Make sure there are enough levels
        needLevel( m_tree.getMaxInsLevel() );
    }
    else
    {
        // Make an element vector
        std::vector<index_t> refVector = this->asElements(boxes, refExt);//std::vector<unsigned> refVector = this->asElements(boxes, refExt);

        // ...and refine
        this->refineElements( refVector );
    }

    // Update the basis (already done by now)
    //update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::unrefine(gsMatrix<T> const & boxes, int refExt)
{
    GISMO_ASSERT(boxes.rows() == d, "refine() needs d rows of boxes.");
    GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provide refine() with them.");

#ifndef NDEBUG
    gsMatrix<T> para = support();
    for(int i = 0; i < boxes.cols()/2; i++)
    {
        for( short_t j = 0; j < d; j++ )
        {
            GISMO_ASSERT( para(j,0) <= boxes(j, 2*i) ,
                          "In refine() the first corner is outside the computational domain.");
            GISMO_ASSERT( para(j,1) >= boxes(j, 2*i+1),
                          "In refine() the second corner is outside the computational domain." );
        }
    }
#endif

    // Make an element vector
    std::vector<index_t> refVector = this->asElementsUnrefine(boxes, refExt);//std::vector<unsigned> refVector = this->asElements(boxes, refExt);

    // ...and refine
    this->unrefineElements( refVector );

    // if( refExt == 0 )
    // {
    //     // If there is no refinement-extension, just use the
    //     // "regular" unrefinement function unrefine( gsMatrix )
    //     // this->unrefine( boxes );

    //     // Make an element vector
    //     std::vector<index_t> refVector = this->asElements(boxes, 0);//std::vector<unsigned> refVector = this->asElements(boxes, refExt);

    //     // ...and refine
    //     this->unrefineElements( refVector );

    //     // Make an element vector
    //     // this->unrefine( boxes );
    // }
    // else
    // {
    //     // Make an element vector
    //     std::vector<index_t> refVector = this->asElements(boxes, refExt);//std::vector<unsigned> refVector = this->asElements(boxes, refExt);

    //     // ...and refine
    //     this->unrefineElements( refVector );
    // }

    // Update the basis (already done by now)
    //update_structure();
}

template<short_t d, class T>
std::vector<index_t> gsHTensorBasis<d,T>::asElements(gsMatrix<T> const & boxes, int refExt) const
{
    // If there is a refinement-extension, we will have to use
    // refineElements( std::vector )
    //
    // Each box will be represented by 2*d+1 entries specifying
    // <level to be refined to>,<lower corner>,<upper corner>
    const int offset = 2*d+1;

    // Initialize vector of size
    // "entries per box" times "number of boxes":
    std::vector<index_t> refVector( offset * boxes.cols()/2 );
    gsMatrix<T> ctr(d,1);

    // Loop over all boxes:
    for(index_t i = 0; i < boxes.cols()/2; i++)
    {
        ctr = ( boxes.col( 2*i ) + boxes.col( 2*i+1) )/2;

        // Compute the level we want to refine to.
        // Note that, if the box extends over several elements,
        // the level at the centerpoint will be taken for reference
        const int refLevel = getLevelAtPoint( ctr ) + 1;

        // Make sure there are enough levels
        needLevel( refLevel );

        for(index_t j = 0; j < boxes.rows();j++)
        {
            // Convert the parameter coordinates to (unique) knot indices
            const gsKnotVector<T> & kv = m_bases[refLevel]->knots(j);
            index_t k1 = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd(),
                                           boxes(j,2*i  ) ) - 1).uIndex();
            index_t k2 = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd()+1,
                                           boxes(j,2*i+1) ) - 1).uIndex();

            // Trivial boxes trigger some refinement
            if ( k1 == k2)
            {
                if (0!=k1) {--k1;}
                ++k2;
            }

            index_t maxKtIndex = kv.size();
            if (m_manualLevels)
            {
                _knotIndexToDiadicIndex(refLevel,j,k1);
                _knotIndexToDiadicIndex(refLevel,j,k2);

                _knotIndexToDiadicIndex(refLevel,j,maxKtIndex);
            }
            // If applicable, add the refinement extension.
            // Note that extending by one cell on level L means
            // extending by two cells in level L+1
            ( k1 < 2*refExt ? k1=0 : k1-=2*refExt );
            ( k2 + 2*refExt >= maxKtIndex ? k2=maxKtIndex-1 : k2+=2*refExt);

            // Store the data...
            refVector[i*offset]       = refLevel;
            refVector[i*offset+1+j]   = k1;
            refVector[i*offset+1+j+d] = k2;
        }
    }
    // gsDebug<<"begin\n";
    // for (std::vector<unsigned>::const_iterator i = refVector.begin(); i != refVector.end(); ++i)
    //     std::cout << *i << ' ';
    // gsDebug<<"end\n";

    return refVector;
}

template<short_t d, class T>
std::vector<index_t> gsHTensorBasis<d,T>::asElementsUnrefine(gsMatrix<T> const & boxes, int refExt) const
{
    // If there is a refinement-extension, we will have to use
    // refineElements( std::vector )
    //
    // Each box will be represented by 2*d+1 entries specifying
    // <level to be refined to>,<lower corner>,<upper corner>
    const int offset = 2*d+1;

    // Initialize vector of size
    // "entries per box" times "number of boxes":
    std::vector<index_t> refVector;
    refVector.reserve( offset * boxes.cols()/2 );
    gsMatrix<T> ctr(d,1);

    // Index for the actual boxes that can be unrefined
    // (so not the ones with level < 0 )
    index_t I = 0;
    // Loop over all boxes:
    for(index_t i = 0; i < boxes.cols()/2; i++)
    {
        ctr = ( boxes.col( 2*i ) + boxes.col( 2*i+1) )/2;

        // Compute the level we want to refine to.
        // Note that, if the box extends over several elements,
        // the level at the centerpoint will be taken for reference
        const int refLevel = getLevelAtPoint( ctr ) - 1;

        // If the level is 0, we cannot coarsen
        if (refLevel < 0) continue;

        refVector.resize(refVector.size() + offset);

        for(index_t j = 0; j < boxes.rows();j++)
        {
            // Convert the parameter coordinates to (unique) knot indices
            const gsKnotVector<T> & kv = m_bases[refLevel+1]->knots(j);
            index_t k1 = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd(),
                                       boxes(j,2*i  ) ) - 1).uIndex();
            index_t k2 = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd()+1,
                                       boxes(j,2*i+1) ) - 1).uIndex();

            // Trivial boxes trigger some refinement
            if ( k1 == k2)
            {
                if (0!=k1) {--k1;}
                ++k2;
            }

            index_t maxKtIndexd = kv.size();
            if (m_manualLevels)
            {
                _knotIndexToDiadicIndex(refLevel,j,k1);
                _knotIndexToDiadicIndex(refLevel,j,k2);

                _knotIndexToDiadicIndex(refLevel,j,maxKtIndexd);
            }

            // If applicable, add the refinement extension.
            ( k1 < refExt ? k1=0 : k1-=refExt );
            ( k2 + refExt >= maxKtIndexd ? k2=maxKtIndexd-1 : k2+=refExt);

            // go one level up;
            k1 /= 2;
            k2 =  k2/2 + (index_t)(k2%2 != 0);

            // Store the data...
            refVector[I*offset]       = refLevel;
            refVector[I*offset+1+j]   = k1;
            refVector[I*offset+1+j+d] = k2;
        }

        I++;
    }

    return refVector;
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::refine(gsMatrix<T> const & boxes)
{
    GISMO_ASSERT(boxes.rows() == d, "refine() needs d rows of boxes.");
    GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provide refine() with them.");

#ifndef NDEBUG
    gsMatrix<T> para = support();
    for(int i = 0; i < boxes.cols()/2; i++)
    {
        for( short_t j = 0; j < d; j++ )
        {
            GISMO_ASSERT( para(j,0) <= boxes(j, 2*i) ,
                          "In refine() the first corner is outside the computational domain.");
            GISMO_ASSERT( para(j,1) >= boxes(j, 2*i+1),
                          "In refine() the second corner is outside the computational domain." );
        }
    }
#endif

    gsVector<index_t,d> k1, k2;
    for(index_t i = 0; i < boxes.cols()/2; i++)
    {
        // 1. Get a small cell containing the box
        const int fLevel = m_bases.size()-1;

        for(index_t j = 0; j < k1.size();j++)
        {
            const gsKnotVector<T> & kv = m_bases.back()->knots(j);
            k1[j] = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd(),
                                      boxes(j,2*i  ) ) - 1).uIndex();
            k2[j] = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd()+1,
                                      boxes(j,2*i+1) ) - 1).uIndex();

            // Trivial boxes trigger some refinement
            if ( k1[j] == k2[j])
            {
                if (0!=k1[j]) {--k1[j];}
                ++k2[j];
            }
        }

        if (m_manualLevels)
        {
            _knotIndexToDiadicIndex(fLevel,k1);
            _knotIndexToDiadicIndex(fLevel,k2);
        }

        // 2. Find the smallest level in which the box is completely contained
        //const int level = m_tree.query3(k1,k2,fLevel) + 1;
        // make sure that the grid is computed ( needLevel(level) )
        //const tensorBasis & tb = tensorLevel(level);
        //GISMO_UNUSED(tb);

        // Sink box
        m_tree.sinkBox(k1, k2, fLevel);
        // Make sure we have enough levels
        needLevel( m_tree.getMaxInsLevel() );
    }

    // Update the basis
    update_structure();
}

// template<short_t d, class T>
// void gsHTensorBasis<d,T>::unrefine(gsMatrix<T> const & boxes)
// {
//     GISMO_ASSERT(boxes.rows() == d, "unrefine() needs d rows of boxes.");
//     GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provide unrefine() with them.");

// #ifndef NDEBUG
//     gsMatrix<T> para = support();
//     for(int i = 0; i < boxes.cols()/2; i++)
//     {
//         for( short_t j = 0; j < d; j++ )
//         {
//             GISMO_ASSERT( para(j,0) <= boxes(j, 2*i) ,
//                           "In unrefine() the first corner is outside the computational domain.");
//             GISMO_ASSERT( para(j,1) >= boxes(j, 2*i+1),
//                           "In unrefine() the second corner is outside the computational domain." );
//         }
//     }
// #endif

//     gsVector<index_t,d> k1, k2;
//     for(index_t i = 0; i < boxes.cols()/2; i++)
//     {
//         // 1. Get a small cell containing the box
//         const int fLevel = m_bases.size()-1;

//         for(index_t j = 0; j < k1.size();j++)
//         {
//             const gsKnotVector<T> & kv = m_bases.back()->knots(j);
//             k1[j] = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd(),
//                                       boxes(j,2*i  ) ) - 1).uIndex();
//             k2[j] = (std::upper_bound(kv.domainUBegin(), kv.domainUEnd()+1,
//                                       boxes(j,2*i+1) ) - 1).uIndex();

//             // Trivial boxes trigger some refinement
//             if ( k1[j] == k2[j])
//             {
//                 if (0!=k1[j]) {--k1[j];}
//                 ++k2[j];
//             }
//         }

//         // 2. Find the smallest level in which the box is completely contained
//         //const int level = m_tree.query3(k1,k2,fLevel) + 1;
//         // make sure that the grid is computed ( needLevel(level) )
//         //const tensorBasis & tb = tensorLevel(level);
//         //GISMO_UNUSED(tb);

//         // Sink box
//         m_tree.raiseBox(k1, k2, fLevel);
//     }

//     // Update the basis
//     // update_structure();
// }

template<short_t d, class T>
void gsHTensorBasis<d,T>::refineBasisFunction(const index_t i)
{
    // Get current level
    const index_t lvl = this->levelOf(i);
    // Get the support endpoints
    gsMatrix<index_t, d, 2>	elements;
    m_bases[lvl]->elementSupport_into(m_xmatrix[lvl][ i - m_xmatrix_offset[lvl] ],
                                          elements);
    point low = elements.col(0);
    point upp = elements.col(1);
    // Advance the indices to one level deeper
    for ( short_t k = 0; k!=d; ++k )
    {
        low[k] = low[k] << 1;
        upp[k] = upp[k] << 1;
    }
    // Insert the domain to the lvl+1 nested domain
    m_tree.insertBox(low,upp,lvl+1);
    // Update the basis
    update_structure();
}


/*
  template<short_t d, class T>
  void gsHTensorBasis<d,T>::refine(gsDomainIterator<T> const & boxes)
  {

  }
*/

template<short_t d, class T>
void gsHTensorBasis<d,T>::refineElements(std::vector<index_t> const & boxes)
{
    point i1;
    point i2;

    GISMO_ASSERT( (boxes.size()%(2*d + 1))==0,
                  "The points did not define boxes properly. The boxes were not added to the basis.");
    for(size_t i = 0; i < (boxes.size())/(2*d+1); i++)
    {
        for( short_t j = 0; j < d; j++ )
        {
            i1[j] = boxes[(i*(2*d+1))+j+1];
            i2[j] = boxes[(i*(2*d+1))+d+j+1];
        }
        insert_box(i1,i2,boxes[i*(2*d+1)]);
    }

    update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::unrefineElements(std::vector<index_t> const & boxes)
{
    point i1;
    point i2;

    GISMO_ASSERT( (boxes.size()%(2*d + 1))==0,
                  "The points did not define boxes properly. The boxes were not added to the basis.");

    for(size_t i = 0; i < (boxes.size())/(2*d+1); i++)
    {
        for( short_t j = 0; j < d; j++ )
        {
            i1[j] = boxes[(i*(2*d+1))+j+1];
            i2[j] = boxes[(i*(2*d+1))+d+j+1];
        }

        m_tree.clearBox(i1,i2, boxes[i*(2*d+1)]);
        // needLevel( m_tree.getMaxInsLevel() );
    }

    // reconstruct the whole tree to fix alignment
    gsHDomain<d> newtree( m_tree.upperCornerIndex() );
    auto leafIt = m_tree.beginLeafIterator();
    for (; leafIt.good(); leafIt.next())
    {
        if ( leafIt.level()>0 )
            newtree.insertBox(leafIt.lowerCorner(),
                              leafIt.upperCorner(), leafIt.level() );
    }
    m_tree = newtree;

    //recompute max-ins-level
    m_tree.computeMaxInsLevel();
    update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::refineSide(const boxSide side, index_t lvl)
{
    const index_t dir = side.direction();
    const index_t par = side.parameter();
    gsMatrix<T> rf = this->support();
    rf(dir,!par) = rf(dir,par);
    for (index_t i = 0; i!=lvl; ++i) // lazy impl., this can be more efficient
        this->refine(rf);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::matchWith(const boundaryInterface & bi,
                                    const gsBasis<T> & other,
                                    gsMatrix<index_t> & bndThis,
                                    gsMatrix<index_t> & bndOther,
                                    index_t offset) const
{
    if( const Self_t * _other = dynamic_cast<const Self_t*>( &other) )
    {
        // tens1 will store the tensor-index on side second(),...
        gsVector<index_t,d> N, tens0, tens1;

        // see if the orientation is preserved on side second()
        const gsVector<bool> dirOrient = bi.dirOrientation();
        const gsVector<index_t> dirMap = bi.dirMap();

        // get the global indices of the basis functions which are
        // active on the interface
        bndThis = this->boundaryOffset( bi.first().side(), offset );

        // this is only for checking whether, at least, both involved
        // bases have the same number of DOF on the interface.
        bndOther= _other->boundaryOffset( bi.second().side(), offset );
        GISMO_ASSERT( bndThis.rows() == bndOther.rows(),
                      "Input error, sizes do not match: "
                      <<bndThis.rows()<<"!="<<bndOther.rows() );
        // bndOther gets overwritten completely, so here is the setZero():
        bndOther.setZero();

        for( index_t i=0; i < bndThis.rows(); i++)
        {
            // get the level of the basis function on side first()
            index_t L = this->levelOf( bndThis(i,0) );
            // get the flat tensor index
            // (i.e., the single-number-index on level L)...
            index_t flat0 = this->flatTensorIndexOf( bndThis(i,0) );
            // ... and change it to the tensor-index.
            tens0 = this->tensorLevel(L).tensorIndex( flat0 );

            // ...flat1 the corresponding flat index
            // (single-number on level)...
            index_t flat1 = 0;
            // ...and cont1 the corresponding continued (global) index.
            index_t cont1 = 0;

            // get the sizes of the components of the tensor-basis on this level,
            // i.e., the sizes of the univariate bases corresponding
            // to the respective coordinate directions
            for( short_t j=0; j < d; j++)
                N[j] = _other->tensorLevel(L).component(j).size();

            // get the tensor-index of the basis function on level L on
            // second() that should be matched with flatp/tens0
            for( short_t j=0; j<d; j++)
            {
                // coordinate direction j on first() gets
                // mapped to direction jj on second()
                index_t jj = dirMap[j];
                // store the respective component of the tensor-index
                tens1[jj] = tens0[j];

                if( jj == bi.second().direction() )
                {
                    // if jj is the direction() of the interface,
                    // however, we need either the first
                    // or last basis function
                    if( bi.second().parameter() ) // true = 1 = end
                        tens1[jj] = N[jj]-(1+offset);
                    else
                        tens1[jj] = offset;
                }
                else
                {
                    // otherwise, check if the orientation is
                    // preserved. If necessary, flip it.
                    if( !dirOrient[j] )
                        tens1[jj] = N[jj]-1 - tens1[jj];
                }

            }

            flat1 = _other->tensorLevel(L).index( tens1 );

            // compute the "continuous" index on second(), i.e., the index
            // in the numbering which is global over all levels.
            cont1 = _other->flatTensorIndexToHierachicalIndex( flat1, L );
            // this is the index that has to be matched with bndThis(i,0)
            bndOther( i, 0 ) = cont1;
        }
        return;
    }
    gsWarn<<"Cannot match with "<< other <<"\n";
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::matchWith(const boundaryInterface & bi,
                                    const gsBasis<T> & other,
                                    gsMatrix<index_t> & bndThis,
                                    gsMatrix<index_t> & bndOther) const
{
    matchWith(bi,other,bndThis,bndOther,0);
}


//protected functions

// Construct the characteristic matrix of level \a level ; i.e., set
// all the matrix entries corresponding to active functions to one and
// the rest to zero.
template<short_t d, class T>
void gsHTensorBasis<d,T>::set_activ1(int level)
{
    typedef typename gsKnotVector<T>::smart_iterator knotIter;

    //gsDebug<<" Setting level "<< level <<"\n";
    point low, upp;

    CMatrix & cmat = m_xmatrix[level];

    // Clear previous entries
    cmat.clear();

    // If a level is to be checked which is larger than
    // the maximum inserted level, nothing need so to be done
    if ( level > static_cast<int>(m_tree.getMaxInsLevel() ) )
        return;


    gsVector<knotIter,d> starts, ends, curr;
    point ind;
    ind[0] = 0; // for d==1: warning: may be used uninitialized in this function (snap-ci)

    for(short_t i = 0; i != d; ++i)
    {
        // beginning of the iteration in i-th direction
        starts[i] = m_bases[level]->knots(i).sbegin() ;
        // end of the iteration in i-th direction
        ends  [i] = m_bases[level]->knots(i).send()-m_deg[i]-1;
    }

    curr = starts;// start iteration
    do
    {
        for(short_t i = 0; i != d; ++i)
        {
            low[i]  = curr[i].uIndex();  // lower left corner of the support of the function
            upp[i]  = (curr[i]+m_deg[i]+1).uIndex(); // upper right corner of the support
            ind[i]  = curr[i].index(); // index of the function in the matrix
        }

        //Here: get knot indices in some standard indexing (eg. dyadic)
        if (m_manualLevels)
        {
            _knotIndexToDiadicIndex(level,low);
            _knotIndexToDiadicIndex(level,upp);
        }

        if ( m_tree.query3(low, upp,level) == level) //if active ????
            cmat.push_unsorted( m_bases[level]->index( ind ) );

    }
    while (  nextLexicographic(curr,starts,ends) ); // while there are some functions (i.e., some combinations of iterators) left

    cmat.sort();

}

template<short_t d, class T>
void gsHTensorBasis<d,T>::functionOverlap(const point & boxLow, const point & boxUpp,
                                          const int level, point & actLow, point & actUpp)
{
    const tensorBasis & tb = *m_bases[level];
    for(short_t i = 0; i != d; ++i)
    {
        actLow[i] = tb.knots(i).lastKnotIndex (boxLow[i]) - m_deg[i];
        actUpp[i] = tb.knots(i).firstKnotIndex(boxUpp[i]) - 1       ;

        // Note aao:
        //actLow[i] = firstKnotIndex(boxLow[i]);
        //actUpp[i] = tb.knots(i).lastKnotIndex(boxUpp[i])-m_deg[i]-1;
    }
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::setActive()
{
    // for(size_t lvl = 0; lvl != m_xmatrix.size(); lvl++)
    //     set_activ1(lvl);
    // return;

    // iterate over leaf-boxes
    //   for all overlapping supports with the box
    //     set obvious to active
    //     for the rest candidates (supp. not fully contained in box ~ !query2)
    //     (equiv: actives on the boundary cells of the box)
    //       query3(supp,box.level) == level (min. is level: no coarser)
    // take care: duplicates from different leaves or adj. cells
    point curr, actUpp;
    gsMatrix<index_t,d,2> elSupp;

    // try: iteration per level
    for ( typename hdomain_type::literator it = m_tree.beginLeafIterator();
          it.good(); it.next() )
    {
        const int lvl = it.level();
        CMatrix & cmat = m_xmatrix[lvl];

        // Get candidate functions
        functionOverlap(it.lowerCorner(), it.upperCorner(), lvl, curr, actUpp);

        do
        {
            const index_t gi = m_bases[lvl]->index( curr );

            // Get element support
            m_bases[lvl]->elementSupport_into(gi, elSupp);

            if ( (elSupp.col(0).array() >= it.lowerCorner().array()).all() &&
                 (elSupp.col(1).array() <= it.upperCorner().array()).all() )
            {
                // to do: all-at-once
                cmat.push_unsorted( gi );
            }
            else
            {
                // Check if active (iff no overlap with level less than lvl)
                if ( m_tree.query3(elSupp.col(0), elSupp.col(1), lvl) == lvl)
                    cmat.push_unsorted( gi );
            }
        }
        while( nextCubePoint(curr, actUpp) );
    }

    for(size_t lvl = 0; lvl != m_xmatrix.size(); ++lvl)
    {
        m_xmatrix[lvl].sort();
        m_xmatrix[lvl].erase( std::unique( m_xmatrix[lvl].begin(), m_xmatrix[lvl].end() ),
                              m_xmatrix[lvl].end() );
    }
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::setActiveToLvl(int level,
                                         std::vector<CMatrix> & x_matrix_lvl) const
{
    x_matrix_lvl.resize(level+1);

    // vectors of iterators. one iterator for each dimension
    gsVector<typename gsKnotVector<T>::smart_iterator,d> starts, ends, curr;
    gsVector<index_t,d> ind;
    ind[0] = 0; // for d==1: warning: may be used uninitialized in this function (snap-ci)
    gsVector<index_t,d> low, upp;
    for(int j =0; j < level+1; j++)
    {
        // Clear previous entries
        x_matrix_lvl[j].clear();

        for(index_t i = 0; i != d; ++i)
        {
            // beginning of the iteration in i-th direction
            starts[i] = m_bases[j]->knots(i).sbegin() ;
            // end of the iteration in i-th direction
            ends  [i] = m_bases[j]->knots(i).send()-m_deg[i]-1;
        }

        curr = starts; // set start of iteration
        do
        {
            for(index_t i = 0; i != d; ++i)
            {
                low[i]  = curr[i].uIndex(); // lower left corner of the support of the function
                upp[i]  = (curr[i]+m_deg[i]+1).uIndex(); // upper right corner of the support
                ind[i]  = curr[i].index(); // index of the function in the matrix
            }
            if(j < level)
            {
                if ( m_tree.query3(low, upp,j) == j)
                { //if active
                    x_matrix_lvl[j].push_unsorted( m_bases[j]->index( ind ) );
                }
            }else{
                if ( m_tree.query3(low, upp,j) >= j)
                { //if active
                    x_matrix_lvl[j].push_unsorted( m_bases[j]->index( ind ) );
                }
            }
        }
        while ( nextLexicographic(curr,starts,ends) ); // while there are some functions (i.e., some combinations of iterators) left

        x_matrix_lvl[j].sort();
    }

}

///private functions
template<short_t d, class T> inline
void gsHTensorBasis<d,T>::insert_box(typename gsHTensorBasis<d,T>::point const & k1,
                                     typename gsHTensorBasis<d,T>::point const & k2,
                                     int lvl )
{
    // Remember box in History (for debugging)
    // m_boxHistory.push_back( box(k1,k2,lvl) );

    m_tree.insertBox(k1,k2, lvl);
    needLevel( m_tree.getMaxInsLevel() );
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::makeCompressed()
{
    // Compress the tree
    // m_tree.makeCompressed();

    while ( ! m_xmatrix_offset[1] )
    {
        delete m_bases.front();
        m_bases.erase( m_bases.begin() );
        m_tree.decrementLevel();
        m_xmatrix.erase( m_xmatrix.begin() );
        m_xmatrix_offset.erase( m_xmatrix_offset.begin() );
    }
    // Note/to do: cleaning up empty levels at the end as well.
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::flatTensorIndexesToHierachicalIndexes(gsSortedVector< int > & indexes,const int level) const
{
    GISMO_ASSERT( static_cast<size_t>(level) < m_xmatrix.size(), "Requested level does not exist.\n");

    CMatrix::const_iterator xmat_pointer = m_xmatrix[level].begin();
    CMatrix::const_iterator xmat_end = m_xmatrix[level].end();
    gsSortedVector< int >::iterator ind_pointer = indexes.begin();
    gsSortedVector< int >::iterator ind_end = indexes.end();
    index_t index = 0;
    while(ind_pointer!=ind_end&&xmat_pointer!=xmat_end)
    {
        if(*ind_pointer<static_cast<int>(*xmat_pointer))
        {
            (*ind_pointer)=-1;
            ++ind_pointer;
        }
        else if(*ind_pointer==static_cast<int>(*xmat_pointer))
        {
            (*ind_pointer)=m_xmatrix_offset[level]+index;
            ++xmat_pointer;
            ++index;
            ++ind_pointer;
        }
        else
        {
            ++xmat_pointer;
            ++index;
        }
    }
    while(ind_pointer!=ind_end)
    {
        (*ind_pointer)=-1;
        ++ind_pointer;
    }
}

template<short_t d, class T>
int gsHTensorBasis<d,T>::flatTensorIndexToHierachicalIndex(index_t index,const int level) const
{
    if( m_xmatrix.size()<=static_cast<size_t>(level) )
        return -1;
    CMatrix::const_iterator it = std::lower_bound(m_xmatrix[level].begin(), m_xmatrix[level].end(), index );
    if(it == m_xmatrix[level].end() || *it != index)
        return -1;
    else
        return m_xmatrix_offset[level] + ( it - m_xmatrix[level].begin() );
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::activeBoundaryFunctionsOfLevel(const unsigned level,const boxSide & s,std::vector<bool>& actives) const
{
    needLevel( level );

    const gsMatrix<index_t> bound = m_bases[level]->boundary(s);
    const index_t sz = bound.rows();
    //gsSortedVector< int > indexes(bound->data(),bound->data()+sz);
    gsSortedVector< int > indexes;
    indexes.resize(sz,-1);
    if(level<=maxLevel())
    {
        for(index_t i = 0;i<sz;++i)
            indexes[i]=(bound)(i,0);
        flatTensorIndexesToHierachicalIndexes(indexes,level);
    }
    actives.resize(indexes.size(),false);
    std::fill (actives.begin(),actives.end(),false);
    for(size_t i = 0;i<indexes.size();i++)
        if(indexes[i]!=-1)
            actives[i]=true;
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::update_structure() // to do: rename as updateHook
{
    // Make sure we have computed enough levels
    needLevel( m_tree.getMaxInsLevel() );

    // Setup the characteristic matrices
    m_xmatrix.clear();
    m_xmatrix.resize( m_tree.getMaxInsLevel()+1 );
    // Compress the tree
    m_tree.makeCompressed();

    for(size_t i = 0; i != m_xmatrix.size(); i ++)
        set_activ1(i);

    // Store all indices of active basis functions to m_matrix
    //setActive();

    // Compute offsets
    m_xmatrix_offset.clear();
    m_xmatrix_offset.reserve(m_xmatrix.size()+1);
    m_xmatrix_offset.push_back(0);
    for (size_t i = 0; i != m_xmatrix.size(); i++)
    {
        m_xmatrix_offset.push_back(
            m_xmatrix_offset.back() + m_xmatrix[i].size() );
    }
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::needLevel(int maxLevel) const
{
    GISMO_ENSURE(!m_manualLevels || (size_t)(maxLevel)<m_uIndices.size(),"Maximum manual level reached, maxLevel = "<<maxLevel<<", m_uIndices.size() = "<<m_uIndices.size());
    // +1 for the initial basis in m_bases
    const int extraLevels = maxLevel + 1 - m_bases.size();
    for ( int i = 0; i < extraLevels; ++i )
    {
        tensorBasis * next_basis = m_bases.back()->clone().release();
        next_basis->uniformRefine(1);
        m_bases.push_back (next_basis); //note: m_bases is mutable
    }
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::initialize_class(gsBasis<T> const&  tbasis)
{
    // Degrees
    //m_deg = tbasis.cwiseDegree();
    m_deg.resize(d);
    for( index_t i = 0; i < d; i++)
        m_deg[i] = tbasis.degree(i);

    // Construct the initial basis
    if ( const tensorBasis * tb2 =
              dynamic_cast<const tensorBasis*>(&tbasis) )
    {
        m_bases.push_back(tb2->clone().release());
    }
    else
    {
        GISMO_ERROR("Cannot construct a Hierarchical basis from "<< tbasis );
    }

    // Initialize the binary tree
    point upp;
    for ( index_t i = 0; i!=d; ++i )
        upp[i] = m_bases[0]->knots(i).numElements();

    m_tree.init(upp);

    // Need one level at least, in case refine(gsMatrix<T> boxes) is called
    this->needLevel(1);
}


template<short_t d, class T>
void gsHTensorBasis<d,T>::active_into(const gsMatrix<T> & u, gsMatrix<index_t>& result) const
{
    gsMatrix<T> currPoint;
    point low, upp, cur;
    const int maxLevel = m_tree.getMaxInsLevel();

    std::vector<std::vector<index_t> > temp_output;//collects the outputs
    temp_output.resize( u.cols() );
    size_t sz = 0;

    //gsMatrix<index_t> activesLvl;

    for(index_t p = 0; p < u.cols(); p++) //for all input points
    {
        currPoint = u.col(p);
        for(short_t i = 0; i != d; ++i)
            low[i] = m_bases[maxLevel]->knots(i).uFind( currPoint(i,0) ).uIndex();

        if (m_manualLevels)
            this->_knotIndexToDiadicIndex(maxLevel,low);

        // Identify the level of the point
        const int lvl = m_tree.levelOf(low, maxLevel);
        for(int i = 0; i <= lvl; i++)
        {
            /*
              m_bases[i]->active_into(currPoint, activesLvl);

              std::set_intersection(m_xmatrix[i].begin(), m_xmatrix[i].end(),
              activesLvl.data(), activesLvl.data() + activesLvl.size(),
              std::back_inserter( temp_output[p] ) );
              +++ Renumbering to H-basis indexing
            */

            m_bases[i]->active_cwise(currPoint, low, upp);
            cur = low;
            do
            {
                CMatrix::const_iterator it =
                    m_xmatrix[i].find_it_or_fail( m_bases[i]->index(cur) );

                if( it != m_xmatrix[i].end() )// if index is found
                {
                    temp_output[p].push_back(
                        this->m_xmatrix_offset[i] + (it - m_xmatrix[i].begin() )
                        );
                }
            }
            while( nextCubePoint(cur,low,upp) );
        }

        // update result size
        if ( temp_output[p].size() > sz )
            sz = temp_output[p].size();
    }

    result.resize(sz, u.cols() );
    for(index_t i = 0; i < result.cols(); i++)
    {
        result.col(i).topRows(temp_output[i].size())
            = gsAsConstVector<index_t>(temp_output[i]);
        result.col(i).bottomRows(sz-temp_output[i].size()).setZero();
    }
}

template<short_t d, class T>
gsMatrix<index_t>  gsHTensorBasis<d,T>::allBoundary( ) const
{
    std::vector<index_t> temp;
    gsVector<index_t, d>  ind;
    for(size_t i = 0; i != m_xmatrix[i].size(); i++)
        for (CMatrix::const_iterator it = m_xmatrix[i].begin();
             it != m_xmatrix[i].end(); it++)
        {
            ind = this->m_bases[i]->tensorIndex(*it);
            for (unsigned j=0; j!=d; ++j )
                if ( (ind[j]==0) || (ind[j]==(this->m_bases[i]->size(j)-1)) )
                {
                    temp.push_back(m_xmatrix_offset[i] + (it-m_xmatrix[i].begin()) );
                    break;
                }
        }
    return makeMatrix<index_t>(temp.begin(),temp.size(),1 );
}

template<short_t d, class T>
gsMatrix<index_t>  gsHTensorBasis<d,T>::
boundaryOffset(boxSide const & s,index_t offset) const
{
    //get information on the side
    const index_t k   = s.direction();
    const bool par = s.parameter();

    std::vector<index_t> temp;
    gsVector<index_t,d>  ind;
    // i goes through all levels of the hierarchical basis
    GISMO_ASSERT(this->maxLevel() < this->m_bases.size(),"Something went wrong: maxLevel() < m_bases.size(), "<<this->maxLevel()<<" < "<<m_bases.size());
    needLevel(m_xmatrix.size()-1);

    for(size_t i = 0; i != m_xmatrix.size(); i++)
    {
        GISMO_ASSERT(static_cast<int>(offset)<this->m_bases[i]->size(k),
                     "Offset ("<<offset<<") cannot be bigger than the amount of basis"
                     "functions orthogonal to Boxside s! ("<<this->m_bases[i]->size(k)<<")");

        index_t r = ( par ? this->m_bases[i]->size(k) - 1 -offset : offset);
        for (CMatrix::const_iterator it = m_xmatrix[i].begin();
             it != m_xmatrix[i].end(); it++)
        {
            ind = this->m_bases[i]->tensorIndex(*it);
            if ( ind[k]==r )
                temp.push_back(
                    m_xmatrix_offset[i] +  (it - m_xmatrix[i].begin() )
                    );
        }
    }
    return makeMatrix<index_t>(temp.begin(),temp.size(),1 );
}

template<short_t d, class T>
gsMatrix<index_t>  gsHTensorBasis<d,T>::
boundaryOffsetLevel(boxSide const & s,index_t offset, index_t level) const
{
    //get information on the side
    //index_t k   = s.direction();
    //bool par = s.parameter();
    return this->m_bases[level]->boundaryOffset(s, offset);
}

template<short_t d, class T>
index_t  gsHTensorBasis<d,T>::
levelAtCorner(boxCorner const & c) const
{
    // Get parametric points of corner
    gsVector<bool> pars;
    c.parameters_into(d,pars);
    // Get the support of the underlying tensor basis
    gsMatrix<T> supp = m_bases.front()->support();
    // Assign the extreme of the support depending on the parametric coordinate
    gsVector<T> vec(supp.rows());
    for (index_t r = 0; r!=supp.rows(); r++)
        vec(r) = supp(r,pars(r));

    return getLevelAtPoint(vec);
}

template<short_t d, class T>
index_t  gsHTensorBasis<d,T>::
functionAtCorner(boxCorner const & c) const
{
    index_t lvl = this->levelAtCorner(c);

    // Get the index of the corner on the level
    index_t index = m_bases[lvl]->functionAtCorner(c);

    // Transform to (continuous) HBspline index.
    return this->flatTensorIndexToHierachicalIndex(index,lvl);
}

template<short_t d, class T>
index_t  gsHTensorBasis<d,T>::
functionAtCorner(boxCorner const & c, index_t level) const
{
    // Get the index of the corner on the level
    index_t index = m_bases[level]->functionAtCorner(c);

    // Transform to (continuous) HBspline index.
    return this->flatTensorIndexToHierachicalIndex(index,level);
}

/*
template<short_t d, class T>
void gsHTensorBasis<d,T>::evalAllDers_into(const gsMatrix<T> & u, int n,
                                           std::vector<gsMatrix<T> >& result, bool sameElement) const;
{
    result.resize(n+1);

}
*/

template<short_t d, class T>
void gsHTensorBasis<d,T>::uniformRefine(int numKnots, int mul, int dir)
{
    GISMO_UNUSED(numKnots);
    GISMO_ASSERT(numKnots == 1, "Only implemented for numKnots = 1");

    GISMO_ASSERT( m_tree.getMaxInsLevel() < static_cast<unsigned>(m_bases.size()),
                  "Problem with max inserted levels: "<< m_tree.getMaxInsLevel()
                  <<"<" << m_bases.size() <<"\n");

    // Keep consistency of finest level
    tensorBasis * last_basis = m_bases.back()->clone().release();
    last_basis->uniformRefine(1,mul,dir);
    m_bases.push_back( last_basis );

    // Delete the first level
    delete m_bases.front();
    m_bases.erase( m_bases.begin() );

    // Lift all indices in the tree by one level
    m_tree.multiplyByTwo();

    update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::uniformCoarsen(int numKnots)
{
    GISMO_UNUSED(numKnots);
    GISMO_ASSERT(numKnots == 1, "Only implemented for numKnots = 1");

    tensorBasis * first_basis = m_bases.front()->clone().release();
    first_basis->uniformCoarsen(1);
    m_bases.insert( m_bases.begin(), first_basis );

    // Delete the last level
    delete m_bases.back();
    m_bases.pop_back();

    // Lift all indices in the tree by one level
    m_tree.divideByTwo();
    // What happens when zero interior knots???????

    update_structure();
}


template<short_t d, class T>
std::vector< std::vector< std::vector<index_t > > > gsHTensorBasis<d,T>::domainBoundariesParams( std::vector< std::vector< std::vector< std::vector< T > > > >& result) const
{
    std::vector< std::vector< std::vector< std::vector<index_t > > > > dummy;
    return domainBoundariesGeneric( dummy, result, false );
}

template<short_t d, class T>
std::vector< std::vector< std::vector<index_t > > > gsHTensorBasis<d,T>::domainBoundariesIndices( std::vector< std::vector< std::vector< std::vector<index_t > > > >& result ) const
{
    std::vector< std::vector< std::vector< std::vector< T > > > > dummy;
    return domainBoundariesGeneric( result, dummy, true );
}


template<short_t d, class T>
std::vector< std::vector< std::vector<index_t > > > gsHTensorBasis<d,T>::domainBoundariesGeneric(std::vector< std::vector< std::vector< std::vector<index_t > > > >& indices,
                                                                                                       std::vector< std::vector< std::vector< std::vector< T > > > >& params,
                                                                                                       bool indicesFlag ) const
{
    indices.clear();
    params.clear();
    std::vector< std::vector< std::vector< int > > > res_aabb;
    std::vector< std::vector< std::vector<index_t > > > res_aabb_unsigned;
    std::vector< std::vector< std::vector< std::vector< index_t > > > > polylines;

    polylines =  this->m_tree.getPolylines();
    res_aabb.resize( polylines.size() );
    // We cannot simply say
    // result = this->m_tree.getPolylines();
    // because the return value of getPolylines() are vectors of ints and we have no implicit conversion of int to T.

    // We want indices/params to be of the same size as polylines. We achieve this here and in the for cycles.
    if( indicesFlag )
        indices.resize( polylines.size() );

    else
        params.resize(polylines.size());

    int maxLevel = static_cast<int>( this->maxLevel() );
    // We precompute the parameter values corresponding to indices of m_maxInsLevel
    // although we don't need them if indicesFlag == true.
    std::vector<T> x_dir(m_bases[maxLevel]->knots(0).unique());
    std::vector<T> y_dir(m_bases[maxLevel]->knots(1).unique());

    for(unsigned int i0 = 0; i0 < polylines.size(); i0++)
    {
        if( indicesFlag )
            indices[i0].resize( polylines[i0].size() );
        else
            params[i0].resize( polylines[i0].size() );

        res_aabb[i0].resize( polylines[i0].size() );
        for(unsigned int i1 = 0; i1 < polylines[i0].size(); i1++)
        {
            if( indicesFlag )
                indices[i0][i1].resize( polylines[i0][i1].size() );
            else
                params[i0][i1].resize( polylines[i0][i1].size() );

            res_aabb[i0][i1].resize( 4 );
            res_aabb[i0][i1][0] = 1000000;
            res_aabb[i0][i1][1] = 1000000;
            res_aabb[i0][i1][2] = -10000000;
            res_aabb[i0][i1][3] = -10000000;
            for(unsigned int i2 = 0; i2 < polylines[i0][i1].size(); i2++)
            {
                if( indicesFlag )
                {
                    indices[i0][i1][i2].resize(4);
                    indices[i0][i1][i2][0] = polylines[i0][i1][i2][0];
                    indices[i0][i1][i2][1] = polylines[i0][i1][i2][1];
                    indices[i0][i1][i2][2] = polylines[i0][i1][i2][2];
                    indices[i0][i1][i2][3] = polylines[i0][i1][i2][3];

                }
                else
                {
                    params[i0][i1][i2].resize(4);
                    // We could as well successively push_back() them but this should be slightly more efficient.
                    params[i0][i1][i2][0] = (x_dir[polylines[i0][i1][i2][0]]);
                    params[i0][i1][i2][1] = (y_dir[polylines[i0][i1][i2][1]]);
                    params[i0][i1][i2][2] = (x_dir[polylines[i0][i1][i2][2]]);
                    params[i0][i1][i2][3] = (y_dir[polylines[i0][i1][i2][3]]);
                }
                if(res_aabb[i0][i1][0]>signed(polylines[i0][i1][i2][0]))
                {
                    res_aabb[i0][i1][0] = polylines[i0][i1][i2][0];
                }
                if(res_aabb[i0][i1][1]>signed(polylines[i0][i1][i2][1]))
                {
                    res_aabb[i0][i1][1] = polylines[i0][i1][i2][1];
                }
                if(res_aabb[i0][i1][2]<signed(polylines[i0][i1][i2][2]))
                {
                    res_aabb[i0][i1][2] = polylines[i0][i1][i2][2];
                }
                if(res_aabb[i0][i1][3]<signed(polylines[i0][i1][i2][3]))
                {
                    res_aabb[i0][i1][3] = polylines[i0][i1][i2][3];
                }
            }
        }
    }

    res_aabb_unsigned.resize(res_aabb.size());
    for (unsigned int i = 0; i < res_aabb.size(); i++)
    {
        res_aabb_unsigned[i].resize(res_aabb[i].size());
        for (unsigned int j = 0; j < res_aabb[i].size(); j++)
        {
            res_aabb_unsigned[i][j].resize(res_aabb[i][j].size());
            for (unsigned int k = 0; k < res_aabb[i][j].size(); k++)
            {
                if(res_aabb[i][j][k]<0)
                    gsWarn << "conversion form signed to unsigned\n";
                res_aabb_unsigned[i][j][k] = res_aabb[i][j][k];
            }
        }
    }
    return res_aabb_unsigned;
}


template<short_t d, class T>
void  gsHTensorBasis<d,T>::transfer(const std::vector<gsSortedVector<index_t> >& old, gsSparseMatrix<T>& result)
{
    // Note: implementation assumes number of old + 1 m_bases exists in this basis
    needLevel( old.size() );

    tensorBasis T_0_copy = this->tensorLevel(0);

    std::vector< gsSparseMatrix<T,RowMajor> > transfer(m_bases.size()-1);
    std::vector<std::vector<T> > knots(d);

    for(size_t i = 1; i < m_bases.size(); ++i)
    {
        //T_0_copy.uniformRefine_withTransfer(transfer[i-1], 1);
        for(unsigned dim = 0; dim != d; ++dim)
        {
            const gsKnotVector<T> & ckv = m_bases[i-1]->knots(dim);
            const gsKnotVector<T> & fkv = m_bases[i  ]->knots(dim);
            ckv.symDifference(fkv, knots[dim]);
            // equivalent (dyadic ref.):
            // ckv.getUniformRefinementKnots(1, knots[dim]);

            //gsDebug << "level: " << i << "\n"
            //        << "direction: " << dim << "\n";
            //gsDebugVar(gsAsMatrix<T>(dirKnots));
        }

        T_0_copy.refine_withTransfer(transfer[i-1], knots);
    }

    // Add missing empty char. matrices
    while ( old.size() >= m_xmatrix.size() )
        m_xmatrix.push_back( gsSortedVector<index_t>() );

    result = this->coarsening_direct(old, m_xmatrix, transfer);

    // This function automatically adds additional characteristic matrices,
    // even if they are not needed.
    // Check whether the characteristic matrices corresponding to the finest
    // levels are actually used. If they are empty, i.e., if there are no
    // active functions on that level, drop them...
    while( m_xmatrix.back().size() == 0 )
        m_xmatrix.pop_back();

    // ...similarly, erase all those fine bases which are actually not used.
    const int sizeDiff = static_cast<int>( m_bases.size() - m_xmatrix.size() );
    if( sizeDiff > 0 )
    {
        freeAll(m_bases.end() - sizeDiff, m_bases.end());
        m_bases.resize(m_xmatrix.size());
    }

    result.makeCompressed();
}

template<short_t d, class T>
void  gsHTensorBasis<d,T>::transfer2(const std::vector<gsSortedVector<index_t> >& old, gsSparseMatrix<T>& result)
{
    // Note: implementation assumes number of old + 1 m_bases exists in this basis
    needLevel( old.size() );

    tensorBasis T_0_copy = this->tensorLevel(0);
    std::vector< gsSparseMatrix<T,RowMajor> > transfer( m_bases.size()-1 );
    std::vector<std::vector<T> > knots(d);

    for(size_t i = 1; i < m_bases.size(); ++i)
    {
        //T_0_copy.uniformRefine_withTransfer(transfer[i], 1);
        for(short_t dim = 0; dim != d; ++dim)
        {
            const gsKnotVector<T> & ckv = m_bases[i-1]->knots(dim);
            const gsKnotVector<T> & fkv = m_bases[i  ]->knots(dim);
            ckv.symDifference(fkv, knots[dim]);
            // equivalent (dyadic ref.):
            // ckv.getUniformRefinementKnots(1, knots[dim]);

            //gsDebug << "level: " << i << "\n"
            //        << "direction: " << dim << "\n";
            //gsDebugVar(gsAsMatrix<T>(dirKnots));
        }
        T_0_copy.refine_withTransfer(transfer[i-1], knots);
    }

    // Add missing empty char. matrices
    while ( old.size() >= m_xmatrix.size())
        m_xmatrix.push_back( gsSortedVector<index_t>() );

    result = this->coarsening_direct2(old, m_xmatrix, transfer);

    result.makeCompressed();
}


template<short_t d, class T>
void gsHTensorBasis<d,T>::increaseMultiplicity(index_t lvl, int dir, T knotValue, int mult)
{
    GISMO_ASSERT( static_cast<size_t>(lvl) < m_xmatrix.size(),
                  "Requested level does not exist.\n");

    if (m_bases[lvl]->knots(dir).has(knotValue))
    {
        for(unsigned int i =lvl;i < m_bases.size();i++)
            m_bases[i]->component(dir).insertKnot(knotValue,mult);
    }
    else
    {
        gsWarn<<"Knot value not in the given knot vector."<<std::endl;
    }

    update_structure();
}


template<short_t d, class T>
void gsHTensorBasis<d,T>::increaseMultiplicity(index_t lvl, int dir, const std::vector<T> & knotValue, int mult)
{
    for(unsigned int k =0; k < knotValue.size(); ++k)
    {
        if (m_bases[lvl]->knots(dir).has(knotValue[k]))
        {
            for(unsigned int i =lvl;i < m_bases.size(); ++i)
                m_bases[i]->component(dir).insertKnot(knotValue[k],mult);
        }
        else
        {
            gsWarn<<"Knot value not in the given knot vector."<<std::endl;
        }
    }
    update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::getBoxesAlongSlice( int dir, T par,std::vector<index_t>& boxes ) const
{
    gsMatrix<index_t> b1,b2;
    gsVector<index_t> level;
    m_tree.getBoxesInLevelIndex(b1,b2,level);
    gsVector<index_t> min,max;
    for(index_t i = 0;i<level.rows();i++)
    {
        min = b1.row(i);
        max = b2.row(i);
        const index_t l = level(i);
        const index_t par_index = m_bases[l]->knots(dir).uFind(par).uIndex();
        if(l>0 && (par_index>=min(dir)) && (par_index<=max(dir)))
        {
            boxes.push_back(l);
            for(index_t j=0;j<min.rows();++j)
                if(j!=dir)
                    boxes.push_back(min(j));
            for(index_t j=0;j<max.rows();++j)
                if(j!=dir)
                    boxes.push_back(max(j));
        }
    }
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::_knotIndexToDiadicIndex(const index_t level, const index_t dir, index_t & knotIndex) const
{
    GISMO_ASSERT(m_manualLevels,"Only works for manual levels");
    knotIndex = m_uIndices[level][dir][knotIndex];
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::_knotIndexToDiadicIndex(const index_t level, gsVector<index_t,d> & knotIndex) const
{
    for (index_t r = 0; r != d; r++)
        this->_knotIndexToDiadicIndex(level,r,knotIndex[r]);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::_diadicIndexToKnotIndex(const index_t level, const index_t dir, index_t & diadicIndex) const
{
    GISMO_ASSERT(m_manualLevels,"Only works for manual levels");
    typename std::vector<index_t>::const_iterator it = std::find_if(m_uIndices[level][dir].begin(),m_uIndices[level][dir].end(),[&diadicIndex](const index_t i) { return i >= diadicIndex; });
    GISMO_ASSERT(it!=m_uIndices[level][dir].end(),"Index not found");
    diadicIndex = std::distance(m_uIndices[level][dir].begin(),it);
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::_diadicIndexToKnotIndex(const index_t level, gsVector<index_t,d> & diadicIndex) const
{
    for (index_t r = 0; r != d; r++)
        this->_diadicIndexToKnotIndex(level,r,diadicIndex[r]);
}


template<short_t d, class T>
void gsHTensorBasis<d,T>::degreeElevate(int const & i, int const dir)
{
    for (size_t level=0;level<m_bases.size();++level)
        m_bases[level]->degreeElevate(i,dir);

    for(unsigned c=0; c<d; ++c)
        m_deg[c]=m_bases[0]->degree(c);

    this->update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::degreeReduce(int const & i, int const dir)
{
    for (size_t level=0;level<m_bases.size();++level)
        m_bases[level]->degreeReduce(i,dir);

    for(unsigned c=0; c<d; ++c)
        m_deg[c]=m_bases[0]->degree(c);

    this->update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::degreeIncrease(int const & i, int const dir)
{
    for (size_t level=0;level<m_bases.size();++level)
        m_bases[level]->degreeIncrease(i,dir);

    for(unsigned c=0; c<d; ++c)
        m_deg[c]=m_bases[0]->degree(c);

    this->update_structure();
}

template<short_t d, class T>
void gsHTensorBasis<d,T>::degreeDecrease(int const & i, int const dir)
{
    for (size_t level=0;level<m_bases.size();++level)
        m_bases[level]->degreeDecrease(i,dir);

    for(unsigned c=0; c<d; ++c)
        m_deg[c]=m_bases[0]->degree(c);

    this->update_structure();
}

template<short_t d, class T>
bool gsHTensorBasis<d,T>::testPartitionOfUnity(const index_t npts, const T tol) const
{
    gsVector<unsigned> np(d);
    np.setConstant(npts);
    gsMatrix<T> supp = this->support();
    gsMatrix<T> grid = gsPointGrid<T>(supp.col(0),supp.col(1),np);
    gsMatrix<T> res;
    this->eval_into(grid,res);
    gsVector<T> sums = res.colwise().sum();
    return ((sums.array()>1-tol && sums.array()<1+tol).all());
}


namespace internal
{

/// Get a HTensorBasis from XML data
template<short_t d, class T>
class gsXml< gsHTensorBasis<d,T> >
{
private:
    gsXml() { }
public:
    GSXML_COMMON_FUNCTIONS(gsHTensorBasis<TMPLA2(d,T)>);
    static std::string tag () { return "Basis"; }
    static std::string type () { return ""; } // tag ?

    static gsHTensorBasis<d,T> * get (gsXmlNode * node)
    {
        gsXmlAttribute * btype = node->first_attribute("type");
        if ( ! btype )
        {
            gsWarn<< "Basis without a type in the xml file.\n";
            return NULL;
        }
        std::string s = btype->value() ;
        if ( s.compare(0, 9, "HBSplineB" , 9 ) == 0 ) // needs correct d as well
            return gsXml< gsHBSplineBasis<d,T> >::get(node);
        if ( s.compare(0, 10,"THBSplineB", 10) == 0 )
            return gsXml< gsTHBSplineBasis<d,T> >::get(node);

        gsWarn<<"gsXmlUtils: gsHTensorBasis: No known basis \""<<s<<"\". Error.\n";
        return NULL;
    }

    static gsXmlNode * put (const gsHTensorBasis<d,T> & obj,
                            gsXmlTree & data )
    {
        const gsBasis<T> * ptr = & obj;

        // Hier. B-splines
        if ( const gsHBSplineBasis<d,T>  * g =
             dynamic_cast<const gsHBSplineBasis<d,T> *>( ptr ) )
            return gsXml< gsHBSplineBasis<d,T> >::put(*g,data);

        // Truncated hier. B-splines
        if ( const gsTHBSplineBasis<d,T>  * g =
             dynamic_cast<const gsTHBSplineBasis<d,T> *>( ptr ) )
            return gsXml< gsTHBSplineBasis<d,T> >::put(*g,data);

        gsWarn<<"gsXmlUtils put: getBasis: No known basis \""<<obj<<"\". Error.\n";
        return NULL;
    }
};

} // namespace internal


} // namespace gismo
