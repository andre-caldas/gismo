/** @file stokes_ieti_example.cpp

    @brief Provides an example for the IETI solver for a Stokes problem

    This example file illustrates how to set up a IETI solver for systems
    of PDEs.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): S. Takacs, J. Sogn
*/

#include <gismo.h>

using namespace gismo;

template<class Container>
gsSparseMatrix<real_t, RowMajor> combinedJumpMatrix(const Container& ietiMapper, index_t k);

template<class Container>
std::vector<index_t> combinedPrimalDofIndices(const Container& ietiMapper, index_t k);

template<class Container>
std::vector<gsSparseVector<real_t>> combinedPrimalConstraints(const Container& ietiMapper, index_t k);

template<class Container>
std::vector<index_t> combinedSkeletonDofs(const Container& ietiMapper, index_t k);

template<class Container>
std::vector<std::vector<gsMatrix<>>> decomposeSolution(const Container& ietiMapper, const std::vector<gsMatrix<>>& solution);

template<class T>
typename gsMultiPatch<T>::uPtr lift3D( const gsMultiPatch<T>& mp, T z );

int main(int argc, char *argv[])
{
    /************** Define command line options *************/

    bool liftGeo = false;
    index_t splitPatches = 0;
    index_t refinements = 1;
    index_t degree = 2;
    real_t tolerance = 1.e-6;
    index_t maxIterations = 300;
    std::string fn;
    bool plot = false;

    gsCmdLine cmd("Solves the Stokes system with an isogeometric discretization using an isogeometric tearing and interconnecting (IETI) solver.");
    cmd.addSwitch("",  "3D",                    "Lift geometry to 3D.", liftGeo);
    cmd.addInt   ("",  "SplitPatches",          "Split every patch that many times in 2^d patches", splitPatches);
    cmd.addInt   ("r", "Refinements",           "Number of uniform h-refinement steps to perform before solving", refinements);
    cmd.addInt   ("p", "Degree",                "Degree of the B-spline discretization space (for pressure)", degree);
    cmd.addReal  ("t", "Solver.Tolerance",      "Stopping criterion for linear solver", tolerance);
    cmd.addInt   ("",  "Solver.MaxIterations",  "Stopping criterion for linear solver", maxIterations);
    cmd.addString("" , "fn",                    "Write solution and used options to file", fn);
    cmd.addSwitch(     "plot",                  "Plot the result with Paraview", plot);

    try { cmd.getValues(argc,argv); } catch (int rv) { return rv; }

    // Use Geometry
    std::string geometry("planar/rectangle_with_disk_hole.xml");
    gsInfo << "Use geometry file planar/rectangle_with_disk_hole.xml.\n";
    if ( ! gsFileManager::fileExists(geometry) )
    {
        gsInfo << "Geometry file could not be found.\n";
        gsInfo << "I was searching in the current directory and in: " << gsFileManager::getSearchPaths() << "\n";
        return EXIT_FAILURE;
    }

    gsInfo << "Run ieti_stokes_example with options:\n" << cmd << std::endl;

    /******************* Define geometry ********************/

    gsInfo << "Define geometry... " << std::flush;

    gsMultiPatch<>::uPtr mpPtr = gsReadFile<>(geometry);
    if (!mpPtr)
    {
        gsInfo << "No geometry found in file " << geometry << ".\n";
        return EXIT_FAILURE;
    }
    if (liftGeo)
        mpPtr = lift3D(*mpPtr,(real_t)(10));
    gsMultiPatch<>& mp = *mpPtr;

    const std::size_t dim = mp.geoDim();

    for (index_t i=0; i<splitPatches; ++i)
    {
        gsInfo << "split patches uniformly... " << std::flush;
        mp = mp.uniformSplit();
    }

    gsInfo << "done.\n";

    /************** Define boundary conditions **************/

    gsInfo << "Define boundary conditions... " << std::flush;

    // The following boundary conditions are set up to make sense for the
    // "rectangle with disk hole"-domain.

    // Functions used to describe boundary data
    gsConstantFunction<> zero( 0.0, dim );
    gsFunctionExpr<> inflow( dim<3 ? "sin(pi*(2+y)/4)" : "sin(pi*(2+y)/4)*z*(10-z)", dim );

    // Boundary conditions
    std::vector<gsBoundaryConditions<>> bc;
    bc.resize(dim+1);

    index_t inlets = 0, outlets = 0, bccount = 0;
    for (gsMultiPatch<>::const_biterator it = mp.bBegin(); it < mp.bEnd(); ++it)
    {
        if (it->patch / math::exp2(mp.geoDim()*splitPatches) == 3 && it->side() == 4) // Inlet
        {
            bc[0].addCondition( *it, condition_type::dirichlet, &inflow );
            for (std::size_t r=1; r<dim; ++r)
                bc[r].addCondition( *it, condition_type::dirichlet, &zero );
            ++inlets;
        }
        else if (it->patch / math::exp2(mp.geoDim()*splitPatches) == 10 && it->side() == 2) // Outlet
        {
            for (std::size_t r=0; r<dim; ++r)
                bc[r].addCondition( *it, condition_type::neumann, &zero );
            ++outlets;
        }
        else
        {
            for (std::size_t r=0; r<dim; ++r)
                bc[r].addCondition( *it, condition_type::dirichlet, &zero );
        }
        ++bccount;
    }
    for (std::size_t r=0; r<dim+1; ++r)
        bc[r].setGeoMap(mp);

    gsInfo << "done: "<<bccount<<" conditions, including "
           <<inlets<<" inlet and "<<outlets<<" outlet, set.\n";

    /************ Setup bases and adjust degree *************/
    //! [Define Bases]
    std::vector<gsMultiBasis<>> mb;
    for (std::size_t r=0; r<dim+1; ++r)
        mb.emplace_back(mp,true);
    const index_t nPatches = mp.nPatches();

    gsInfo << "Setup bases and adjust degree (Taylor-Hood)... " << std::flush;

    for (std::size_t r=0; r<dim; ++r)
    {
        for (index_t k=0; k<nPatches; ++k)
            mb[r][k].setDegreePreservingMultiplicity(degree+1);
        for ( index_t i=0; i<refinements; ++i )
            mb[r].uniformRefine();
        for (index_t k=0; k<nPatches; ++k)
            mb[r][k].reduceContinuity(1);
    }

    for (index_t k=0; k<nPatches; ++k)
        mb[dim][k].setDegreePreservingMultiplicity(degree);
    for ( index_t i=0; i<refinements; ++i )
        mb[dim].uniformRefine();

    gsInfo << "done.\n";
    //! [Define Bases]

    /********* Setup assembler and assemble matrix **********/

    gsInfo << "Setup assembler and assemble matrix for patch... " << std::flush;

    //! [Define Mapper]
    std::vector<gsIetiMapper<>> ietiMapper;
    ietiMapper.resize(dim+1);

    // We start by setting up a global assembler for each component to
    // obtain a dof mapper and the Dirichlet data
    for (std::size_t r=0; r<dim+1; ++r)
    {
        // Velocity is C^0 continuous, but pressure discontinuous.
        const index_t continuity = r<dim ? 0 : -1;
        gsExprAssembler<> assembler;
        gsExprAssembler<>::space u = assembler.getSpace(mb[r]);
        u.setup(bc[r], dirichlet::interpolation, continuity);
        ietiMapper[r].init(mb[r], u.mapper(), u.fixedPart());
    }
    //! [Define Mapper]

    // Compute the jump matrices (Fully redundant, exclude corners)
    for (std::size_t r=0; r<dim+1; ++r)
        ietiMapper[r].computeJumpMatrices(true, true);

    // We tell the ieti mapper which primal constraints we want; calling
    // more than one such function is possible.
    //! [Define Primals]
    for (std::size_t r=0; r<dim; ++r)
    {
        ietiMapper[r].cornersAsPrimals();
        ietiMapper[r].interfaceAveragesAsPrimals(mp,dim-1);
    }
    ietiMapper[dim].interfaceAveragesAsPrimals(mp,dim);
    //! [Define Primals]

    //! [Define System]
    // The ieti system does not have a special treatment for the
    // primal dofs. They are just one more subdomain
    gsIetiSystem<> ieti;
    ieti.reserve(nPatches+1);

    // The scaled Dirichlet preconditioner is independent of the
    // primal dofs.
    gsScaledDirichletPrec<> prec;
    prec.reserve(nPatches);

    // Setup the primal system, which needs to know the number of primal dofs.
    //! [Define System]
    index_t nPrimals = 0;
    for (std::size_t r=0; r<dim+1; ++r)
        nPrimals += ietiMapper[r].nPrimalDofs();

    //! [Define System2]
    gsPrimalSystem<> primal(nPrimals);
    //! [Define System2]
    primal.setEliminatePointwiseConstraints(false);

    for (index_t k=0; k<nPatches; ++k)
    {
        gsInfo << "[" << k << "] " << std::flush;

        // We use the local variants of everything
        gsMultiPatch<>                      mp_local = mp[k];
        std::vector<gsMultiBasis<>>         mb_local;
        std::vector<gsBoundaryConditions<>> bc_local(dim+1);
        mb_local.reserve(dim+1);
        for (std::size_t r=0; r<dim+1; ++r)
        {
            mb_local.push_back(mb[r][k]);
            bc[r].getConditionsForPatch(k,bc_local[r]);
            bc_local[r].setGeoMap(mp_local);
        }

        // We assemble the stiffness matrix with the expression assembler
        typedef gsExprAssembler<>::geometryMap geometryMap;
        typedef gsExprAssembler<>::variable    variable;
        typedef gsExprAssembler<>::space       space;
        typedef gsExprAssembler<>::solution    solution;

        gsExprAssembler<> assembler(dim+1,dim+1);
        assembler.setIntegrationElements(mb_local[0]);
        gsExprEvaluator<> ev(assembler);
        geometryMap G = assembler.getMap(mp_local);

        // Velocity space
        std::vector<std::remove_cv<space>::type> v;
        for (std::size_t r=0; r<dim; ++r)
        {
            v.push_back(assembler.getSpace(mb_local[r],1,r));
            v[r].setup(bc_local[r], dirichlet::interpolation, 0);
            ietiMapper[r].initFeSpace(v[r],k);
        }

        // Presure space
        space p = assembler.getSpace(mb_local[dim],1,dim);
        p.setup(bc_local[dim], dirichlet::interpolation, 0);
        ietiMapper[dim].initFeSpace(p,k);

        assembler.initSystem();
        //! [Assemble]
        for (std::size_t r=0; r<dim; ++r)
        {
            assembler.assemble( igrad(v[r], G)   * igrad(v[r], G).tr()   * meas(G) ,
                                igrad(v[r],G)[r] * p.tr()                * meas(G) ,
                                p                * igrad(v[r],G)[r].tr() * meas(G) );
        }
        //! [Assemble]

        // Fetch data
        gsSparseMatrix<>                 localMatrix  = assembler.matrix();
        gsMatrix<>                       localRhs      = assembler.rhs();
        //! [Jump Matrix]
        gsSparseMatrix<real_t,RowMajor>  jumpMatrix    = combinedJumpMatrix(ietiMapper, k);
        //! [Jump Matrix]

        // For preconditioning, we only use the Poisson part
        {
            //! [Preconditioner]
            index_t sz = 0;
            for (std::size_t r=0; r<dim; ++r)
                sz += ietiMapper[r].jumpMatrix(k).cols();
            gsSparseMatrix<> velocityJumpMatrix = jumpMatrix.block(0,0,jumpMatrix.rows(),sz);
            gsSparseMatrix<> poissonMatrix = localMatrix.block(0,0,sz,sz);

            std::vector<index_t> skeletonDofs      = combinedSkeletonDofs(ietiMapper,k);
            gsScaledDirichletPrec<>::Blocks blocks = gsScaledDirichletPrec<>::matrixBlocks( poissonMatrix, skeletonDofs );

            prec.addSubdomain(
                gsScaledDirichletPrec<>::restrictJumpMatrix( velocityJumpMatrix, skeletonDofs ).moveToPtr(),
                gsScaledDirichletPrec<>::schurComplement( blocks, makeSparseCholeskySolver(blocks.A11) )
            );
            //! [Preconditioner]
        }

        // Collecting the primal DoFs
        //! [Primals]
        std::vector<index_t> primalDofIndices                 = combinedPrimalDofIndices(ietiMapper,k);
        std::vector<gsSparseVector<real_t>> primalConstraints = combinedPrimalConstraints(ietiMapper,k);
        //! [Primals]

        //! [Register]
        // This function writes back to jumpMatrix, localMatrix, and localRhs,
        // so it must be called after prec.addSubdomain().
        primal.handleConstraints(primalConstraints, primalDofIndices, jumpMatrix, localMatrix, localRhs);

        // Register local problem to ieti system
        ieti.addSubdomain(jumpMatrix.moveToPtr(),
                          makeMatrixOp(localMatrix),
                          give(localRhs),
                          makeSparseLUSolver(localMatrix)
                         );
        //! [Register]

    } // End of local assemblers

    // Add the primal problem
    {
        //! [Register primal]
        gsLinearOperator<>::Ptr localSolver = makeSparseLUSolver(primal.localMatrix());

        ieti.addSubdomain(
            primal.jumpMatrix().moveToPtr(),
            makeMatrixOp(primal.localMatrix().moveToPtr()),
            give(primal.localRhs()),
            localSolver
        );
        //! [Register primal]
    }
    gsInfo << "done.\n";


    /**************** Setup solver and solve ****************/
    gsInfo << "Setup solver and solve... \n"
        "    Setup multiplicity scaling... " << std::flush;

    // Tell the preconditioner to set up the scaling
    prec.setupMultiplicityScaling();

    gsInfo << "done.\n    Setup CG solver and solve... \n" << std::flush;

    // Solve for Lagrange multipliers
    gsMatrix<> lambda;
    lambda.setRandom( ieti.nLagrangeMultipliers(), 1 );

    gsMatrix<> errorHistory;

    //! [Solve]
    gsConjugateGradient<> PCG( ieti.schurComplement(), prec.preconditioner() );
    PCG.setCalcEigenvalues(true);
    PCG.setOptions(cmd.getGroup("Solver"));
    PCG.solveDetailed( ieti.rhsForSchurComplement(), lambda, errorHistory);
    //! [Solve]

    // Report on behavior of solver
    const index_t iter = errorHistory.rows()-1;
    const bool success = errorHistory(iter,0) < PCG.tolerance();
    if (success)
        gsInfo << "        Reached desired tolerance after " << iter << " iterations:\n        ";
    else
        gsInfo << "        Did not reach desired tolerance after " << iter << " iterations:\n        ";
    if (errorHistory.rows() < 20)
        gsInfo << errorHistory.transpose() << "\n\n";
    else
        gsInfo << errorHistory.topRows(5).transpose() << " ... " << errorHistory.bottomRows(5).transpose()  << "\n\n";
    gsInfo << "        Calculated condition number is " << PCG.getConditionNumber() << ".";


    gsInfo << "\n    Reconstruct solution from Lagrange multipliers... " << std::flush;

    // Now, we want to reconstruct the solution
    std::vector<std::vector<gsMatrix<>>> solutionPatches = decomposeSolution(ietiMapper,
                                                               primal.distributePrimalSolution(
                                                                   ieti.constructSolutionFromLagrangeMultipliers(lambda)
                                                               )
                                                           );
    std::vector<gsMatrix<>> solutions;
    for (std::size_t r=0; r<ietiMapper.size(); ++r)
        solutions.push_back( ietiMapper[r].constructGlobalSolutionFromLocalSolutions(solutionPatches[r]));

    gsInfo << "done.\nDone.\n";

    /******************** Print end Exit ********************/
    if (!fn.empty())
    {
        gsFileData<> fd;
        std::time_t time = std::time(NULL);
        fd.add(cmd);
        for (std::size_t r=0; r<ietiMapper.size(); ++r)
            fd.add(solutions[r]);
        fd.addComment(std::string("stokes_ieti_example   Timestamp:")+std::ctime(&time));
        fd.save(fn);
        gsInfo << "Write solution to file " << fn << ".\n";
    }

    if (plot)
    {
        for (std::size_t r=0; r<ietiMapper.size(); ++r)
        {
            const char* filenames[] = { "ieti_velocity_x", "ieti_velocity_y", "ieti_velocity_z", "ieti_pressure" };
            const char* filename = filenames[(r<dim&&r<3)?r:3];
            gsInfo << "Write Paraview data to file \"" << filename << ".pvd\".\n";
            gsMultiPatch<> mpsol;
            for (index_t k=0; k<nPatches; ++k)
                mpsol.addPatch( mb[r][k].makeGeometry( ietiMapper[r].incorporateFixedPart(k, solutionPatches[r][k]) ) );
            gsWriteParaview<>( gsField<>( mp, mpsol ), filename, dim < 3 ? 1000 : 10000);
        }
    }

    if (!plot&&fn.empty())
    {
        gsInfo << "No output created, re-run with --plot to get a ParaView "
                  "file containing the solution or --fn to write solution to xml file.\n";
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Helper functions

template<typename Iterator>
typename std::iterator_traits<Iterator>::value_type blockDiagonal( Iterator begin, Iterator end )
{
    typedef typename std::iterator_traits<Iterator>::value_type sparseMatrix;
    typedef typename sparseMatrix::Scalar T;
    typedef typename sparseMatrix::Index Index;
    Index nz = 0;
    for (Iterator sm_it=begin; sm_it!=end; ++sm_it)
        nz += sm_it->nonZeros();
    gsSparseEntries<T> se;
    se.reserve(nz);
    Index rows = 0, cols = 0;
    for (Iterator sm_it=begin; sm_it!=end; ++sm_it)
    {
        const sparseMatrix& sm = *sm_it;
        for(Index j=0; j<sm.outerSize(); ++j)
            for(typename sparseMatrix::InnerIterator it(sm,j); it; ++it)
                se.add(rows+it.row(), cols+it.col(), it.value());
        rows += sm.rows();
        cols += sm.cols();
    }
    sparseMatrix result(rows, cols);
    result.setFrom(se);
    return result;
}

template<class Container>
gsSparseMatrix<real_t, RowMajor> combinedJumpMatrix(const Container& ietiMapper, index_t k)
{
    std::vector<gsSparseMatrix<real_t, RowMajor>> jumpMatrices;
    for (std::size_t r=0; r<ietiMapper.size(); ++r)
        jumpMatrices.push_back(ietiMapper[r].jumpMatrix(k));
    return blockDiagonal(jumpMatrices.begin(), jumpMatrices.end());
}


template<class Container>
std::vector<index_t> combinedPrimalDofIndices(const Container& ietiMapper, index_t k)
{
    std::vector<index_t> primalDofIndices;
    index_t offset = 0;
    for (std::size_t i=0; i<ietiMapper.size(); ++i)
    {
        const std::vector<index_t>& ind = ietiMapper[i].primalDofIndices(k);
        for (std::size_t j=0; j<ind.size(); ++j)
            primalDofIndices.push_back(ind[j]+offset);
        offset += ietiMapper[i].nPrimalDofs();
    }
    return primalDofIndices;
}

template<class Container>
std::vector<gsSparseVector<real_t>> combinedPrimalConstraints(const Container& ietiMapper, index_t k)
{
    std::vector<gsSparseVector<real_t>> primalConstraints;
    index_t pre = 0, rows = 0;
    for (std::size_t i=0; i<ietiMapper.size(); ++i)
        rows += ietiMapper[i].jumpMatrix(k).cols();

    for (std::size_t i=0; i<ietiMapper.size(); ++i)
    {
        const std::vector<gsSparseVector<real_t>>& cond = ietiMapper[i].primalConstraints(k);
        for (std::size_t j=0; j<cond.size(); ++j)
        {
            gsSparseVector<real_t> tmp(rows);
            for (gsSparseVector<real_t>::InnerIterator it(cond[j]); it; ++it)
                tmp[pre + it.row()] = it.value();
            primalConstraints.push_back(give(tmp));
        }
        pre += ietiMapper[i].jumpMatrix(k).cols();
    }
    return primalConstraints;
}

template<class Container>
std::vector<index_t> combinedSkeletonDofs(const Container& ietiMapper, index_t k)
{
    std::vector<index_t> skeletonDofs;
    index_t offset = 0;
    for (std::size_t i=0; i<ietiMapper.size(); ++i)
    {
        const std::vector<index_t>& sd = ietiMapper[i].skeletonDofs(k);
        for (std::size_t j = 0; j<sd.size(); ++j)
            skeletonDofs.push_back(sd[j]+offset);
        offset += ietiMapper[i].jumpMatrix(k).cols();
    }
    return skeletonDofs;
}

template<class Container>
std::vector<std::vector<gsMatrix<>>> decomposeSolution(const Container& ietiMapper, const std::vector<gsMatrix<>>& solution)
{
    std::vector<std::vector<gsMatrix<>>> result(ietiMapper.size());
    for (std::size_t r=0; r<ietiMapper.size(); ++r)
        result[r].reserve(solution.size());
    for (std::size_t k=0; k<solution.size(); ++k)
    {
        index_t offset = 0;
        for (std::size_t r=0; r<ietiMapper.size(); ++r)
        {
            const index_t sz = ietiMapper[r].jumpMatrix(k).cols();
            result[r].push_back(solution[k].block(offset,0,sz,1));
            offset += ietiMapper[r].jumpMatrix(k).cols();
        }
    }
    return result;
}

template<class T>
typename gsMultiPatch<T>::uPtr lift3D( const gsMultiPatch<T>& mp, T z )
{
    std::vector<gsGeometry<T>*> geos;
    geos.reserve(mp.nPatches());
    for (typename std::vector<gsGeometry<T>*>::const_iterator it = mp.begin();
        it != mp.end(); ++it)
    {
        const gsTensorBSpline<2,T>* tb = dynamic_cast<const gsTensorBSpline<2,T>*>(*it);
        if (tb)
        {
            geos.push_back(gsNurbsCreator<T>::lift3D(*tb, z).release());
            continue;
        }
        const gsTensorNurbs<2,T>* tn = dynamic_cast<const gsTensorNurbs<2,T>*>(*it);
        if (tn)
        {
            geos.push_back(gsNurbsCreator<T>::lift3D(*tn, z).release());
            continue;
        }
        GISMO_ENSURE(false, "lift3D only available for gsTensorBSpline<2,T> and gsTensorNurbs<2,T>.");
    }
    typename gsMultiPatch<T>::uPtr result = memory::make_unique(new gsMultiPatch<T>(geos));
    result->computeTopology();
    return result;
}
