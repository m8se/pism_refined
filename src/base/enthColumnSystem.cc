// Copyright (C) 2009 Andreas Aschwanden and Ed Bueler
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include "enthColumnSystem.hh"
#include "pism_const.hh"   // e.g. MASK_FLOATING and PismModMask()
#include "enthalpyHelper.hh"

#define DEBUGVERB 2

enthSystemCtx::enthSystemCtx(int my_Mz, int my_Mbz)
      : columnSystemCtx(my_Mz + my_Mbz - 1) {
  Mz = my_Mz;
  Mbz = my_Mbz;
  k0 = Mbz - 1; // max size nmax of system is Mz + k0 = Mz + Mbz - 1
  // set flags to indicate nothing yet set
  initAllDone = false;
  indicesValid = false;
  schemeParamsValid = false;
  surfBCsValid = false;
  basalBCsValid = false;
  // set values so we can check if init was called on all
  dx = -1;
  dy = -1;
  dtTemp = -1;
  dzEQ = -1;
  dzbEQ = -1;
  ice_rho = -1;
  ice_c   = -1;
  ice_k   = -1;
  bed_thermal_rho = -1;
  bed_thermal_c   = -1;
  bed_thermal_k   = -1;
  Enth = NULL;
  Tb = NULL;
  u = NULL;
  v = NULL;
  w = NULL;
  Sigma = NULL;
  Enth3 = NULL;
}


PetscErrorCode enthSystemCtx::initAllColumns() {
  // check whether each parameter & pointer got set
  if (dx <= 0.0) { SETERRQ(2,"un-initialized dx in enthSystemCtx"); }
  if (dy <= 0.0) { SETERRQ(3,"un-initialized dy in enthSystemCtx"); }
  if (dtTemp <= 0.0) { SETERRQ(4,"un-initialized dtTemp in enthSystemCtx"); }
  if (dzEQ <= 0.0) { SETERRQ(5,"un-initialized dzEQ in enthSystemCtx"); }
  if (dzbEQ <= 0.0) { SETERRQ(6,"un-initialized dzbEQ in enthSystemCtx"); }
  if (ice_rho <= 0.0) { SETERRQ(7,"un-initialized ice_rho in enthSystemCtx"); }
  if (ice_c <= 0.0) { SETERRQ(8,"un-initialized ice_c_p in enthSystemCtx"); }
  if (ice_k <= 0.0) { SETERRQ(9,"un-initialized ice_k in enthSystemCtx"); }
  if (bed_thermal_rho <= 0.0) { SETERRQ(10,"un-initialized bed_thermal_rho in enthSystemCtx"); }
  if (bed_thermal_c <= 0.0) { SETERRQ(11,"un-initialized bed_thermal_c_p in enthSystemCtx"); }
  if (bed_thermal_k <= 0.0) { SETERRQ(12,"un-initialized bed_thermal_k in enthSystemCtx"); }
  if (Enth == NULL) { SETERRQ(13,"un-initialized pointer T in enthSystemCtx"); }
  if (Tb == NULL) { SETERRQ(14,"un-initialized pointer Tb in enthSystemCtx"); }
  if (u == NULL) { SETERRQ(15,"un-initialized pointer u in enthSystemCtx"); }
  if (v == NULL) { SETERRQ(16,"un-initialized pointer v in enthSystemCtx"); }
  if (w == NULL) { SETERRQ(17,"un-initialized pointer w in enthSystemCtx"); }
  if (Sigma == NULL) { SETERRQ(18,"un-initialized pointer Sigma in enthSystemCtx"); }
  if (Enth3 == NULL) { SETERRQ(19,"un-initialized pointer T3 in enthSystemCtx"); }
  // set derived constants
  nuEQ = dtTemp / dzEQ;
  rho_c_I = ice_rho * ice_c;
  rho_c_br = bed_thermal_rho * bed_thermal_c;
  rho_c_av = (dzEQ * rho_c_I + dzbEQ * rho_c_br) / (dzEQ + dzbEQ);
  iceK = ice_k / rho_c_I;
  iceR = iceK * dtTemp / PetscSqr(dzEQ);
  brK = bed_thermal_k / rho_c_br;
  brR = brK * dtTemp / PetscSqr(dzbEQ);
  rho_c_ratio = rho_c_I / rho_c_av;
  dzav = 0.5 * (dzEQ + dzbEQ);
  iceReff = ice_k * dtTemp / (rho_c_av * dzEQ * dzEQ);
  brReff = bed_thermal_k * dtTemp / (rho_c_av * dzbEQ * dzbEQ);
  // done
  initAllDone = true;
  return 0;
}


PetscErrorCode enthSystemCtx::view() {
  PetscErrorCode ierr;
  
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n\nVIEWING enthSystemCtx:\n"); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "  initAllDone = %d\n",initAllDone); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "  dx,dy,dtTemp,dzEQ,dzbEQ = %8.2f,%8.2f,%10.3e,%8.2f,%8.2f\n",
                     dx,dy,dtTemp,dzEQ,dzbEQ); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "  ice_rho,ice_c,ice_k = %10.3e,%10.3e,%10.3e\n",
                     ice_rho,ice_c,ice_k); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "  bed_thermal_rho,bed_thermal_c,bed_thermal_k = %10.3e,%10.3e,%10.3e\n",
                     bed_thermal_rho,bed_thermal_c,bed_thermal_k); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "  nuEQ,rho_c_I,rho_c_br,rho_c_av = %10.3e,%10.3e,%10.3e,%10.3e\n",
                     nuEQ,rho_c_I,rho_c_br,rho_c_av); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "  iceK,iceR,brK,brR = %10.3e,%10.3e,%10.3e,%10.3e\n",
                     iceK,iceR,brK,brR); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "  rho_c_ratio,dzav,iceReff,brReff =  = %10.3e,%10.3e,%10.3e,%10.3e\n\n\n",
                     rho_c_ratio,dzav,iceReff,brReff); CHKERRQ(ierr);
  return 0;
}


PetscErrorCode enthSystemCtx::setIndicesThisColumn(
                  PetscInt my_i, PetscInt my_j, PetscInt my_ks) {
  if (!initAllDone) {  SETERRQ(2,
     "setIndicesThisColumn() should only be called after initAllColumns() in enthSystemCtx"); }
  if (indicesValid) {  SETERRQ(3,
     "setIndicesThisColumn() called twice in same column (?) in enthSystemCtx"); }
  i = my_i;
  j = my_j;
  ks = my_ks;
  indicesValid = true;
  return 0;
}


PetscErrorCode enthSystemCtx::setSchemeParamsThisColumn(
                     PetscScalar my_mask, bool my_isMarginal, PetscScalar my_lambda) {
  if (!initAllDone) {  SETERRQ(2,
     "setSchemeParamsThisColumn() should only be called after initAllColumns() in enthSystemCtx"); }
  if (schemeParamsValid) {  SETERRQ(3,
     "setSchemeParamsThisColumn() called twice (?) in enthSystemCtx"); }
  mask = my_mask;
  isMarginal = my_isMarginal;
  lambda = my_lambda;
  schemeParamsValid = true;
  return 0;
}


PetscErrorCode enthSystemCtx::setSurfaceBoundaryValuesThisColumn(PetscScalar my_Ts) {
  if (!initAllDone) {  SETERRQ(2,
     "setSurfaceBoundaryValuesThisColumn() should only be called after initAllColumns() in enthSystemCtx"); }
  if (surfBCsValid) {  SETERRQ(3,
     "setSurfaceBoundaryValuesThisColumn() called twice (?) in enthSystemCtx"); }
  Ts = my_Ts;
  surfBCsValid = true;
  return 0;
}


PetscErrorCode enthSystemCtx::setBasalBoundaryValuesThisColumn(
                     PetscScalar my_Ghf, PetscScalar my_Tshelfbase, PetscScalar my_Rb) {
  if (!initAllDone) {  SETERRQ(2,
     "setIndicesThisColumn() should only be called after initAllColumns() in enthSystemCtx"); }
  if (basalBCsValid) {  SETERRQ(3,
     "setBasalBoundaryValuesThisColumn() called twice (?) in enthSystemCtx"); }
  Ghf = my_Ghf;
  Tshelfbase = my_Tshelfbase;
  Rb = my_Rb;
  basalBCsValid = true;
  return 0;
}


PetscErrorCode enthSystemCtx::solveThisColumn(PetscScalar **x) {
  PetscErrorCode ierr;
  if (!initAllDone) {  SETERRQ(2,
     "solveThisColumn() should only be called after initAllColumns() in enthSystemCtx"); }
  if (!indicesValid) {  SETERRQ(3,
     "solveThisColumn() should only be called after setIndicesThisColumn() in enthSystemCtx"); }
  if (!schemeParamsValid) {  SETERRQ(3,
     "solveThisColumn() should only be called after setSchemeParamsThisColumn() in enthSystemCtx"); }
  if (!surfBCsValid) {  SETERRQ(3,
     "solveThisColumn() should only be called after setSurfaceBoundaryValuesThisColumn() in enthSystemCtx"); }
  if (!basalBCsValid) {  SETERRQ(3,
     "solveThisColumn() should only be called after setBasalBoundaryValuesThisColumn() in enthSystemCtx"); }

/*  LISTED HERE TO RECALL WHAT THEY MEAN:
  nuEQ = dtTemp / dzEQ;
  rho_c_I = ice_rho * ice_c;
  // rho_c_br = bed_thermal_rho * bed_thermal_c;
  rho_c_av = (dzEQ * rho_c_I + dzbEQ * rho_c_br) / (dzEQ + dzbEQ);
  iceK = ice_k / rho_c_I;
  iceR = iceK * dtTemp / PetscSqr(dzEQ);
  brK = bed_thermal_k / rho_c_br;
  // brR = brK * dtTemp / PetscSqr(dzbEQ);
  rho_c_ratio = rho_c_I / rho_c_av;
  dzav = 0.5 * (dzEQ + dzbEQ);
  iceReff = ice_k * dtTemp / (rho_c_av * dzEQ * dzEQ);
  brReff = bed_thermal_k * dtTemp / (rho_c_av * dzbEQ * dzbEQ);
*/

  if (Mbz > 1) { // bedrock present: build k=0:Mbz-2 eqns
    // gives O(\Delta t,\Delta z^2) convergence in Test K for equal spaced grid;
    // note L[0] not an allocated location:
    D[0] = (1.0 + 2.0 * brR);
    U[0] = - 2.0 * brR;  
    rhs[0] = Tb[0] + 2.0 * dtTemp * Ghf / (rho_c_br * dzbEQ);

    // bedrock only; pure vertical conduction problem
    for (PetscInt k=1; k < k0; k++) {
      L[k] = -brR;
      D[k] = 1.0 + 2.0 * brR;
      U[k] = -brR;
      rhs[k] = Tb[k];
    }
  }

// FIXME FROM HERE ON AT LEAST
#if 0
  // bottom part of ice (and top of bedrock in some cases): k=k0=Mbz-1 eqn
  if (ks == 0) { // no ice; set T[0] to surface temp if grounded
    if (k0 > 0) { L[k0] = 0.0; } // note L[0] not allocated 
    D[k0] = 1.0;
    U[k0] = 0.0;
    // if floating and no ice then worry only about bedrock temps
    if (PismModMask(mask) == MASK_FLOATING) {
      // essentially no ice but floating ... ask PISMOceanCoupler
      rhs[k0] = Tshelfbase;
      // FIXME: split k0 into two grid points?
    } else { // top of bedrock sees atmosphere
      rhs[k0] = Ts; 
    }
  } else { // ks > 0; there is ice
    planeStar ss;
    ierr = T3->getPlaneStarZ(i,j,0.0,&ss);
    const PetscScalar UpTu = (u[0] < 0) ? u[0] * (ss.ip1 -  ss.ij) / dx :
                                          u[0] * (ss.ij  - ss.im1) / dx;
    const PetscScalar UpTv = (v[0] < 0) ? v[0] * (ss.jp1 -  ss.ij) / dy :
                                          v[0] * (ss.ij  - ss.jm1) / dy;
    // for w, always difference *up* from base, but make it implicit
    if (PismModMask(mask) == MASK_FLOATING) {
      // just apply Dirichlet condition to base of column of ice in an ice shelf
      if (k0 > 0) { L[k0] = 0.0; } // note L[0] not allocated 
      D[k0] = 1.0;
      U[k0] = 0.0;
      rhs[k0] = Tshelfbase; // set by PISMOceanCoupler
    } else { 
      // there is *grounded* ice; ice/bedrock interface; from FV across interface
      rhs[k0] = T[0] + dtTemp * (Rb / (rho_c_av * dzav));
      if (!isMarginal) {
        rhs[k0] += dtTemp * rho_c_ratio * 0.5 * (Sigma[0] / rho_c_I);
        // WARNING: subtle consequences of finite volume argument across interface
        rhs[k0] -= dtTemp * rho_c_ratio * (0.5 * (UpTu + UpTv));
      }
      const PetscScalar AA = dtTemp * rho_c_ratio * w[0] / (2.0 * dzEQ);
      if (Mbz > 1) { // there is bedrock; apply upwinding if w[0]<0,
                     // otherwise ignore advection; note 
                     // jump in diffusivity coefficient
        L[k0] = - brReff;
        if (w[0] >= 0.0) {  // velocity upward
          D[k0] = 1.0 + iceReff + brReff;
          U[k0] = - iceReff;
        } else { // velocity downward
          D[k0] = 1.0 + iceReff + brReff - AA;
          U[k0] = - iceReff + AA;
        }
      } else { // no bedrock; apply geothermal flux here
        // L[k0] = 0.0;  (note this is not an allocated location!) 
        if (w[0] >= 0.0) {  // velocity upward
          D[k0] = 1.0 + 2.0 * iceR;
          U[k0] = - 2.0 * iceR;
        } else { // velocity downward
          D[k0] = 1.0 + 2.0 * iceR - AA;
          U[k0] = - 2.0 * iceR + AA;
        }
        rhs[k0] += 2.0 * dtTemp * Ghf / (rho_c_I * dzEQ);
      }
    }
  }

  // generic ice segment: build k0+1:k0+ks-1 eqns
  for (PetscInt k = 1; k < ks; k++) {
    planeStar ss;
    ierr = Enth3->getPlaneStarZ(i,j,k * dzEQ,&ss);
    const PetscScalar UpEnthu = (u[k] < 0) ? u[k] * (ss.ip1 -  ss.ij) / dx :
                                             u[k] * (ss.ij  - ss.im1) / dx;
    const PetscScalar UpEnthv = (v[k] < 0) ? v[k] * (ss.jp1 -  ss.ij) / dy :
                                             v[k] * (ss.ij  - ss.jm1) / dy;
    const PetscScalar AA = nuEQ * w[k];      
    if (w[k] >= 0.0) {  // velocity upward
      L[k0+k] = - iceR - AA * (1.0 - lambda/2.0);
      D[k0+k] = 1.0 + 2.0 * iceR + AA * (1.0 - lambda);
      U[k0+k] = - iceR + AA * (lambda/2.0);
    } else {  // velocity downward
      L[k0+k] = - iceR - AA * (lambda/2.0);
      D[k0+k] = 1.0 + 2.0 * iceR - AA * (1.0 - lambda);
      U[k0+k] = - iceR + AA * (1.0 - lambda/2.0);
    }
    rhs[k0+k] = T[k];
    if (!isMarginal) {
      rhs[k0+k] += dtTemp * (Sigma[k] / rho_c_I - UpTu - UpTv);
    }
  }
      
  // surface b.c.
  if (ks>0) {
    L[k0+ks] = 0.0;
    D[k0+ks] = 1.0;
    // ignore U[k0+ks]
    rhs[k0+ks] = Ts;
  }
#endif

  // mark column as done
  indicesValid = false;
  schemeParamsValid = false;
  surfBCsValid = false;
  basalBCsValid = false;

  // solve it; note melting not addressed yet
  return solveTridiagonalSystem(k0+ks+1,x);
}

