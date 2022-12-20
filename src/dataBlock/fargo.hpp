// ***********************************************************************************
// Idefix MHD astrophysical code
// Copyright(C) Geoffroy R. J. Lesur <geoffroy.lesur@univ-grenoble-alpes.fr>
// and other code contributors
// Licensed under CeCILL 2.1 License, see COPYING for more information
// ***********************************************************************************

#ifndef DATABLOCK_FARGO_HPP_
#define DATABLOCK_FARGO_HPP_

#include <vector>
#include "idefix.hpp"
#ifdef WITH_MPI
  #include "mpi.hpp"
#endif

// Forward class hydro declaration
#include "physics.hpp"
template <typename Phys> class Fluid;
using Hydro = Fluid<Physics>;
class DataBlock;

using FargoVelocityFunc = void (*) (DataBlock &, IdefixArray2D<real> &);



class Fargo {
 public:
  enum FargoType {none, userdef, shearingbox};
  void Init(Input &, DataBlock*);  // Initialisation
  void ShiftSolution(const real t, const real dt);  // Effectively shift the solution
  void SubstractVelocity(const real);
  void AddVelocity(const real);
  void EnrollVelocity(FargoVelocityFunc);
  void StoreToScratch();
  void CheckMaxDisplacement();
  void ShowConfig();

 private:
  friend Hydro;
  DataBlock *data;
  Hydro *hydro;
  FargoType type{none};                 // By default, Fargo is disabled

  IdefixArray2D<real> meanVelocity;
  IdefixArray4D<real> scrhUc;
  IdefixArray4D<real> scrhVs;

#ifdef WITH_MPI
  Mpi mpi;                      // Fargo-specific MPI layer
#endif

  std::vector<int> beg;
  std::vector<int> end;
  std::vector<int> nghost;
  int maxShift;                         //< maximum number of cells along which we plan to shift.
  real dtMax{0};                        //< Maximum allowable dt for a given Fargo velocity
                                        //< when domain decomposition is enabled
  bool velocityHasBeenComputed{false};
  bool haveDomainDecomposition{false};
  void GetFargoVelocity(real);
  FargoVelocityFunc fargoVelocityFunc{NULL};  // The user-defined fargo velocity function
};

#endif // DATABLOCK_FARGO_HPP_
