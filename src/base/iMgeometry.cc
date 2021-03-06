// Copyright (C) 2004-2012 Jed Brown, Ed Bueler and Constantine Khroulev
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

#include <cmath>
#include <cstring>
#include <petscdmda.h>

#include "iceModel.hh"
#include "Mask.hh"
#include "PISMOcean.hh"
#include "PISMSurface.hh"
#include "PISMStressBalance.hh"

//! \file iMgeometry.cc Methods of IceModel which update and maintain consistency of ice sheet geometry.


//! Update the surface elevation and the flow-type mask when the geometry has changed.
/*!
  This procedure should be called whenever necessary to maintain consistency of geometry.

  For instance, it should be called when either ice thickness or bed elevation change.
  In particular we always want \f$h = H + b\f$ to apply at grounded points, and, on the
  other hand, we want the mask to reflect that the ice is floating if the flotation
  criterion applies at a point.

  Also calls the (PIK) routines which remove icebergs, to avoid stress balance
  solver problems associated to not-attached-to-grounded ice.
*/
PetscErrorCode IceModel::updateSurfaceElevationAndMask() {
  PetscErrorCode ierr;

  ierr = update_mask(); CHKERRQ(ierr);
  ierr = update_surface_elevation(); CHKERRQ(ierr);
		 

  if (config.get_flag("kill_icebergs")) {
    ierr = killIceBergs(); CHKERRQ(ierr);
  }

  return 0;
}


PetscErrorCode IceModel::update_mask() {
  PetscErrorCode ierr;

  if (ocean == PETSC_NULL) {
    SETERRQ(grid.com, 1, "PISM ERROR: ocean == PETSC_NULL");
  }
  PetscReal sea_level;
  ierr = ocean->sea_level_elevation(sea_level); CHKERRQ(ierr);

  GeometryCalculator gc(sea_level, config);
  MaskQuery mask(vMask);

  ierr =    vH.begin_access(); CHKERRQ(ierr);
  ierr =  vbed.begin_access(); CHKERRQ(ierr);
  ierr = vMask.begin_access(); CHKERRQ(ierr);

  PetscInt GHOSTS = 2;
  for (PetscInt   i = grid.xs - GHOSTS; i < grid.xs+grid.xm + GHOSTS; ++i) {
    for (PetscInt j = grid.ys - GHOSTS; j < grid.ys+grid.ym + GHOSTS; ++j) {
      vMask(i, j) = gc.mask(vbed(i, j), vH(i,j));
    } // inner for loop (j)
  } // outer for loop (i)

  ierr =    vH.end_access(); CHKERRQ(ierr);
  ierr =  vbed.end_access(); CHKERRQ(ierr);
  ierr = vMask.end_access(); CHKERRQ(ierr);

  return 0;
}

PetscErrorCode IceModel::update_glmask() {
  PetscErrorCode ierr;
  if (ocean == PETSC_NULL) {
    SETERRQ(grid.com, 1, "PISM ERROR: ocean == PETSC_NULL");
  }

  //MaskQuery mask(vMask);
 
ierr =  vMask.begin_access(); CHKERRQ(ierr);
ierr = vGLMask[0]->begin_access(); CHKERRQ(ierr);
ierr = vGLMask[1]->begin_access(); CHKERRQ(ierr);
ierr = vGLMask[0]->copy_to(*vGLMask[1]); CHKERRQ(ierr);	
  
  for (PetscInt   i = grid.xs ; i < grid.xs+grid.xm ; ++i) {
    for (PetscInt j = grid.ys ; j < grid.ys+grid.ym ; ++j) {
	PetscBool found= PETSC_FALSE;
	if (vMask(i,j)==2){
		for (PetscInt k=-1;k<2; ++k){
			for (PetscInt l=-1;l<2; ++l){
			if ((k==0)&&(l==0))continue;
			if (vMask(i+k,j+l)==3){
 			 (*vGLMask[0])(i, j) =1; found= PETSC_TRUE; break;}
			}// l for loop
		} // k for loop
	}//if grounded
	if (found==PETSC_FALSE)(*vGLMask[0])(i, j)=0;
    } // inner for loop (j)
  } // outer for loop (i)
// Adding direct neighbors of grounding line cells
	//TODO
// Identifying non grounding line neighbors of grounding line cells
	for (PetscInt   i = grid.xs ; i < grid.xs+grid.xm ; ++i) {
    for (PetscInt j = grid.ys ; j < grid.ys+grid.ym ; ++j) {
	if ((*vGLMask[0])(i,j)!=1){
		for (PetscInt k=-1;k<2; ++k){
			for (PetscInt l=-1;l<2; ++l){
			if ((k==0)&&(l==0))continue;
			if ((*vGLMask[0])(i+k,j+l)==1){
 			 (*vGLMask[0])(i, j) =2; }
			}// l for loop
		} // k for loop
	}//if not groundingline
    } // inner for loop (j)
  } // outer for loop (i)
	
  ierr =  vMask.end_access(); CHKERRQ(ierr);
  ierr = vGLMask[0]->end_access(); CHKERRQ(ierr);
  ierr = vGLMask[1]->begin_access(); CHKERRQ(ierr);
  return 0;
}

PetscErrorCode IceModel::update_surface_elevation() {
  PetscErrorCode ierr;

  MaskQuery mask(vMask);

  if (ocean == PETSC_NULL) {  SETERRQ(grid.com, 1, "PISM ERROR: ocean == PETSC_NULL");  }
  PetscReal sea_level;
  ierr = ocean->sea_level_elevation(sea_level); CHKERRQ(ierr);

  GeometryCalculator gc(sea_level, config);

  ierr =    vh.begin_access(); CHKERRQ(ierr);
  ierr =    vH.begin_access(); CHKERRQ(ierr);
  ierr =  vbed.begin_access(); CHKERRQ(ierr);
  ierr = vMask.begin_access(); CHKERRQ(ierr);

  PetscInt GHOSTS = 2;
  for (PetscInt   i = grid.xs - GHOSTS; i < grid.xs+grid.xm + GHOSTS; ++i) {
    for (PetscInt j = grid.ys - GHOSTS; j < grid.ys+grid.ym + GHOSTS; ++j) {
      // take this opportunity to check that vH(i, j) >= 0
      if (vH(i, j) < 0) {
        SETERRQ2(grid.com, 1, "Thickness negative at point i=%d, j=%d", i, j);
      }
      vh(i, j) = gc.surface(vbed(i, j), vH(i, j));
    }
  }

  ierr =    vh.end_access(); CHKERRQ(ierr);
  ierr =    vH.end_access(); CHKERRQ(ierr);
  ierr =  vbed.end_access(); CHKERRQ(ierr);
  ierr = vMask.end_access(); CHKERRQ(ierr);

  return 0;
}


PetscErrorCode IceModel::update_refined_variable(IceModelVec2S &var, IceModelVec2S *var_ref) {
	PetscErrorCode ierr;
	ierr =  var.begin_access(); CHKERRQ(ierr);           
	ierr =  var_ref->begin_access(); CHKERRQ(ierr);
	ierr =  vGLMask[0]->begin_access(); CHKERRQ(ierr); 
    for (PetscInt   i = grid.xs ; i < grid.xs+grid.xm ; ++i) {
       for (PetscInt j = grid.ys ; j < grid.ys+grid.ym ; ++j) {
			if((( (*vGLMask[0])(i,j)==1)||((*vGLMask[0])(i,j)==2))&&((*vGLMask[1])(i,j)==0||(*vGLMask[1])(i,j)==2)){
              for (PetscInt   k = i*refinement ;k < (i+1)*refinement ; ++k) {
  	             for (PetscInt l = j*refinement ; l < (j+1)*refinement; ++l) {  
					(*var_ref)(k,l)=var(i,j); 
				   } // "l" for loop
			  }//"k" for loop
			}
			} // "j" for loop
	   } //"i" for loop
	ierr =  vGLMask[0]->end_access(); CHKERRQ(ierr); 
	ierr =  var.end_access(); CHKERRQ(ierr);           
	ierr =  var_ref->end_access(); CHKERRQ(ierr);
return 0;
}

//Refill Variables with recalculated refined values
PetscErrorCode IceModel::update_unrefined_variable(IceModelVec2S &var, IceModelVec2S *var_ref) {
PetscErrorCode ierr;
ierr =  vGLMask[0]->begin_access(); CHKERRQ(ierr);
ierr =  var.begin_access(); CHKERRQ(ierr);           
ierr =  var_ref->begin_access(); CHKERRQ(ierr);
 for (PetscInt   i = grid.xs ; i < grid.xs+grid.xm ; ++i) {
       for (PetscInt j = grid.ys ; j < grid.ys+grid.ym ; ++j) {	
 if((*vGLMask[0])(i,j)==1) {
			 PetscScalar temp=0;
			 for (PetscInt   k = i*refinement ;k < (i+1)*refinement ; ++k) {
  	             for (PetscInt l = j*refinement ; l < (j+1)*refinement; ++l) {
					temp+=(*var_ref)(k,l);
				   } // "l" for loop
			  }//"k" for loop
			  var(i,j)=temp/(refinement*refinement);
			}
		} // "j" for loop
 } //"i" for loop
ierr =  vGLMask[0]->end_access(); CHKERRQ(ierr);
ierr =  var.end_access(); CHKERRQ(ierr);           
ierr =  var_ref->end_access(); CHKERRQ(ierr);
return 0;
}

//! \brief Adjust ice flow through interfaces of the cell i,j.
/*!
 *
 * From the point of view of the code solving the mass continuity equation
 * there are 4 kinds of cells: "grounded ice", "floating ice", "ice-free land",
 * and "ice-free ocean". This makes 16 kinds of interfaces between grid cells.

 * In some of the cases PISM computes a SIA flux (or the SSA velocity) across
 * an interface that ice should not cross. (A silly example: there should be no
 * flow between two adjacent ice-free cells, even if a stress balance solver
 * computes velocity components for the whole computational domain. Please see
 * comments below for more examples.)

 * Note that 6 cases are paired. This is crucial: to be consistent (and
 * conserve mass) the interface between A and B has to get the same treatment
 * as the one between B and A.
 *
 * @param[in] mask Mask used to check for icy/ice-free and floatation
 * @param[in,out] SSA_velocity SSA velocity to be adjusted
 * @param[in,out] SIA_flux SIA flux to be adjusted
 */
void IceModel::adjust_flow(planeStar<int> mask,
                           planeStar<PetscScalar> &SSA_velocity,
                           planeStar<PetscScalar> &SIA_flux) {

  // Prepare to loop over neighbors:
  // directions
  PISM_Direction dirs[] = {North, East, South, West};
  // mask values
  int mask_current = mask.ij;
  Mask M;

  // Loop over neighbors:
  for (int n = 0; n < 4; ++n) {
    PISM_Direction direction = dirs[n];
    int mask_neighbor = mask[direction];

    // Only one of the cases below applies at any given time/location, so
    // "continuing" the for-loop allows us to avoid checking conditions we know
    // will fail. This also means that the code executed in *all* cases should
    // go here and not after if-conditions.

    // Case 1: Flow between grounded_ice and grounded_ice.
    if (M.grounded_ice(mask_current) && M.grounded_ice(mask_neighbor)) {
      // no adjustment; kept for completeness
      continue;
    }


    // Cases 2 and 3: Flow between grounded_ice and floating_ice.
    if ((M.grounded_ice(mask_current) && M.floating_ice(mask_neighbor)) ||
        (M.floating_ice(mask_current) && M.grounded_ice(mask_neighbor))) {
      // no adjustment
      continue;
    }


    // Cases 4 and 5: Flow between grounded_ice and ice_free_land.
    if ((M.grounded_ice(mask_current) && M.ice_free_land(mask_neighbor)) ||
        (M.ice_free_land(mask_current) && M.grounded_ice(mask_neighbor))) {
      // no adjustment
      continue;
    }


    // Cases 6 and 7: Flow between grounded_ice and ice_free_ocean.
    if ((M.grounded_ice(mask_current) && M.ice_free_ocean(mask_neighbor)) ||
        (M.ice_free_ocean(mask_current) && M.grounded_ice(mask_neighbor))) {
      // no adjustment
      continue;
    }

    // Case 8: Flow between floating_ice and floating_ice.
    if (M.floating_ice(mask_current) && M.floating_ice(mask_neighbor)) {
      // disable SIA flow
      SIA_flux[direction] = 0.0;
      continue;
    }


    // Cases 9 and 10: Flow between floating_ice and ice_free_land.
    if ((M.floating_ice(mask_current) && M.ice_free_land(mask_neighbor)) ||
        (M.ice_free_land(mask_current) && M.floating_ice(mask_neighbor))) {
      // disable all flow

      // This ensures that an ice shelf does not climb up a cliff.

      // FIXME: we should check surface elevations instead of using the mask,
      // to allow an ice shelf to advance up a coast gradually rising from the
      // sea level (or similar).

      SIA_flux[direction] = 0.0;
      SSA_velocity[direction] = 0.0;
      continue;
    }


    // Cases 11 and 12: Flow between floating_ice and ice_free_ocean.
    if ((M.floating_ice(mask_current) && M.ice_free_ocean(mask_neighbor)) ||
        (M.ice_free_ocean(mask_current) && M.floating_ice(mask_neighbor))) {

      SIA_flux[direction] = 0.0;

      // The SSA flow may be later adjusted by the code implementing the
      // partially-filled cell parameterization.
      continue;
    }

    // Case 13: Flow between ice_free_land and ice_free_land.
    if (M.ice_free_land(mask_current) && M.ice_free_land(mask_neighbor)) {

      SIA_flux[direction] = 0.0;
      SSA_velocity[direction] = 0.0;
      continue;
    }


    // Cases 14 and 15: Flow between ice_free_land and ice_free_ocean.
    if ((M.ice_free_land(mask_current) && M.ice_free_ocean(mask_neighbor)) ||
        (M.ice_free_ocean(mask_current) && M.ice_free_land(mask_neighbor))) {

      SIA_flux[direction] = 0.0;
      SSA_velocity[direction] = 0.0;
      continue;
    }

    // Case 16: Flow between ice_free_ocean and ice_free_ocean.
    if (M.ice_free_ocean(mask_current) && M.ice_free_ocean(mask_neighbor)) {

      SIA_flux[direction] = 0.0;
      SSA_velocity[direction] = 0.0;
      continue;
    }
  } // end of the loop over neighbors (n)

}

//! \brief Compute fluxes through interfaces of a cell i,j.
/*!
 * This method implements two steps:
 *
 * 1) Compute SSA velocities through interfaces of a cell by averaging values
 * from regular grid neighbors, making sure that velocities from ice-free areas
 * are not used.
 *
 * Note that the input parameter \c input_velocity contains both components of
 * the velocity field in the neighborhood of i,j, while \c output_velocity
 * contains \b scalars: projections of velocity vectors onto normals to
 * corresponding cell interfaces.
 *
 * The SIA flux \c input_flux is computed on the staggered grid by SIAFD, so
 * the loop below just copies it to \c output_flux.
 *
 * 2) Adjust the flow using the mask by calling adjust_flow().
 *
 * @param[in] dirichlet_bc true if Dirichlet B.C. are set.
 * @param[in] i i-index of the current cell
 * @param[in] j j-index of the current cell
 * @param[in] in_SSA_velocity SSA velocity on the regular grid in the neighborhood of i,j
 * @param[in] in_SIA_flux SIA flux on the staggered grid (at interfaces of the cell i,j)
 * @param[out] out_SSA_velocity SSA velocities through interfaces of the cell i,j
 * @param[out] out_SIA_flux SIA flux through interfaces of the cell i,j
 */
void IceModel::cell_interface_fluxes(bool dirichlet_bc,
                                     int i, int j,
                                     planeStar<PISMVector2> in_SSA_velocity,
                                     planeStar<PetscScalar> in_SIA_flux,
                                     planeStar<PetscScalar> &out_SSA_velocity,
                                     planeStar<PetscScalar> &out_SIA_flux) {

  planeStar<int> mask = vMask.int_star(i,j);
  PISM_Direction dirs[4] = {North, East, South, West};
  Mask M;

  planeStar<int> bc_mask;
  planeStar<PISMVector2> bc_velocity;
  if (dirichlet_bc) {
    bc_mask = vBCMask.int_star(i,j);
    bc_velocity = vBCvel.star(i,j);
  }

  out_SSA_velocity.ij = 0.0;
  out_SIA_flux.ij = 0.0;

  for (int n = 0; n < 4; ++n) {
    PISM_Direction direction = dirs[n];
    int mask_current = mask.ij,
      mask_neighbor = mask[direction];

    // The in_SIA_flux is already on the staggered grid, so we can just
    // copy it to out_SIA_flux:
    out_SIA_flux[direction] = in_SIA_flux[direction];

    // Compute the out_SSA_velocity (SSA):
    if (M.icy(mask_current) && M.icy(mask_neighbor)) {

      // Case 1: both sides of the interface are icy
      if (direction == East || direction == West)
        out_SSA_velocity[direction] = 0.5 * (in_SSA_velocity.ij.u + in_SSA_velocity[direction].u);
      else
        out_SSA_velocity[direction] = 0.5 * (in_SSA_velocity.ij.v + in_SSA_velocity[direction].v);

    } else if (M.icy(mask_current) && M.ice_free(mask_neighbor)) {

      // Case 2: icy cell next to an ice-free cell
      if (direction == East || direction == West)
        out_SSA_velocity[direction] = in_SSA_velocity.ij.u;
      else
        out_SSA_velocity[direction] = in_SSA_velocity.ij.v;

    } else if (M.ice_free(mask_current) && M.icy(mask_neighbor)) {

      // Case 3: ice-free cell next to icy cell
      if (direction == East || direction == West)
        out_SSA_velocity[direction] = in_SSA_velocity[direction].u;
      else
        out_SSA_velocity[direction] = in_SSA_velocity[direction].v;

    } else if (M.ice_free(mask_current) && M.ice_free(mask_neighbor)) {

      // Case 4: both sides of the interface are ice-free
      out_SSA_velocity[direction] = 0.0;

    }

    // The Dirichlet B.C. case:
    if (dirichlet_bc) {

      if (bc_mask.ij == 1 && bc_mask[direction] == 1) {

        // Case 1: both sides of the interface are B.C. locations: average from
        // the regular grid onto the staggered grid.
        if (direction == East || direction == West)
          out_SSA_velocity[direction] = 0.5 * (bc_velocity.ij.u + bc_velocity[direction].u);
        else
          out_SSA_velocity[direction] = 0.5 * (bc_velocity.ij.v + bc_velocity[direction].v);

      } else if (bc_mask.ij == 1 && bc_mask[direction] == 0) {

        // Case 2: at a Dirichlet B.C. location
        if (direction == East || direction == West)
          out_SSA_velocity[direction] = bc_velocity.ij.u;
        else                    // North or South
          out_SSA_velocity[direction] = bc_velocity.ij.v;

      } else if (bc_mask.ij == 0 && bc_mask[direction] == 1) {

        // Case 3: next to a Dirichlet B.C. location
        if (direction == East || direction == West)
          out_SSA_velocity[direction] = bc_velocity[direction].u;
        else                  // North or South
          out_SSA_velocity[direction] = bc_velocity[direction].v;

      } else {
        // Case 4: elsewhere.
        // No Dirichlet B.C. adjustment here.
      }

    } // end of "if (dirichlet_bc)"

  } // end of the loop over neighbors

  adjust_flow(mask, out_SSA_velocity, out_SIA_flux);

}






//! Update the thickness from the diffusive flux and sliding velocity, and the surface and basal mass balance rates.
/*!
  The partial differential equation describing the conservation of mass in the
  map-plane (parallel to the geoid) is
  \f[ \frac{\partial H}{\partial t} = M - S - \nabla\cdot \mathbf{q} \f]
  where
  \f[ \mathbf{q} = \bar{\mathbf{U}} H. \f]
  In these equations \f$H\f$ is the ice thickness,
  \f$M\f$ is the surface mass balance (accumulation or ablation), \f$S\f$ is the
  basal mass balance (e.g. basal melt or freeze-on), and \f$\bar{\mathbf{U}}\f$ is
  the vertically-averaged horizontal velocity of the ice.  This procedure uses
  this conservation of mass equation to update the ice thickness.

  The PISMSurfaceModel IceModel::surface provides \f$M\f$.  The
  PISMOceanModel IceModel::ocean provides \f$S\f$ at locations below
  floating ice (ice shelves).

  Even if we regard the map-plane flux as defined by the formula
  \f$\mathbf{q} = \bar{\mathbf{U}} H\f$, the flow of ice is at least somewhat
  diffusive in almost all cases.  In the non-sliding SIA model it
  is exactly true that \f$\mathbf{q} = - D \nabla h\f$.  In the current method the
  flux is split into the part from the diffusive non-sliding SIA model
  and a part which is a less-diffusive, presumably membrane-stress-dominated
  2D advective velocity, which generally describes sliding:
  \f[ \mathbf{q} = - D \nabla h + \mathbf{U}_b H.\f]
  Here \f$D\f$ is the (positive, scalar) effective diffusivity of the non-sliding
  SIA and \f$\mathbf{U}_b\f$ is the less-diffusive sliding velocity.
  We interpret \f$\mathbf{U}_b\f$ as a basal sliding velocity in the hybrid.

  The main ice-dynamical inputs to this method are identified in these lines,
  which get outputs from PISMStressBalance *stress_balance:
  \code
  IceModelVec2Stag *Qdiff;
  stress_balance->get_diffusive_flux(Qdiff);
  IceModelVec2V *vel_advective;
  stress_balance->get_advective_2d_velocity(vel_advective);
  \endcode
  The diffusive flux \f$-D\nabla h\f$ is thus stored in \c IceModelVec2Stag
  \c *Qdiff while the less-diffusive velocity \f$\mathbf{U}_b\f$ is stored in
  \c IceModelVec2V \c *vel_advective.

  The methods used here are first-order and explicit in time.  The derivatives in
  \f$\nabla \cdot (D \nabla h)\f$ are computed by centered finite difference
  methods.  The diffusive flux \c Qdiff is already stored on the staggered grid
  and it is differenced in a centered way here.  The time-stepping for this part
  of the explicit scheme is controlled by equation (25) in [\ref BBL], so that
  \f$\Delta t \sim \Delta x^2 / \max D\f$; see also [\ref MortonMayers].

  The divergence of the flux from velocity \f$\mathbf{U}_b\f$ is computed by
  the upwinding technique [equation (25) in \ref Winkelmannetal2011] which
  is the donor cell upwind method [\ref LeVeque].
  The CFL condition for this advection scheme is checked; see
  computeMax2DSlidingSpeed() and determineTimeStep().  This method implements the
  direct-superposition (PIK) hybrid which adds the SSA velocity to the SIA velocity
  [equation (15) in \ref Winkelmannetal2011].  The hybrid described by equations
  (21) and (22) in \ref BBL is no longer used.

  Checks are made which can generate zero thickness according to minimal calving
  relations, specifically the mechanisms turned-on by options \c -ocean_kill and
  \c -float_kill.

We also compute total ice fluxes in kg s-1 at 3 interfaces:

  \li the ice-atmosphere interface: gets surface mass balance rate from
      PISMSurfaceModel *surface,
  \li the ice-ocean interface at the bottom of ice shelves: gets ocean-imposed
      basal melt rate from PISMOceanModel *ocean, and
  \li the ice-bedrock interface: gets basal melt rate from IceModelVec2S vbmr.

A unit-conversion occurs for all three quantities, from ice-equivalent m s-1
to kg s-1.  The sign convention about these fluxes is that positve flux means
ice is being \e added to the ice fluid volume at that interface.

These quantities should be understood as <i>instantaneous at the beginning of
the time-step.</i>  Multiplying by dt will \b not necessarily give the
corresponding change from the beginning to the end of the time-step.

FIXME:  The calving rate can be computed by post-processing:
dimassdt = surface_ice_flux + basal_ice_flux + sub_shelf_ice_flux + discharge_flux_mass_rate + nonneg_rule_flux

Removed commented-out code using the coverage ratio to compute the surface mass
balance contribution (to reduce clutter). Please see the commit 26330a7 and
earlier. (CK)
*/

PetscErrorCode IceModel::massContExplicitStep() {

	
  PetscErrorCode ierr;
  PetscScalar
    // totals over the processor's domain:
    proc_H_to_Href_flux = 0,
    proc_Href_to_H_flux = 0,
    proc_float_kill_flux = 0,
    proc_grounded_basal_ice_flux = 0,
    proc_nonneg_rule_flux = 0,
    proc_ocean_kill_flux = 0,
    proc_sub_shelf_ice_flux = 0,
    proc_sum_divQ_SIA = 0,
    proc_sum_divQ_SSA = 0,
    proc_surface_ice_flux = 0,
    // totals over all processors:
    total_H_to_Href_flux = 0,
    total_Href_to_H_flux = 0,
    total_float_kill_flux = 0,
    total_grounded_basal_ice_flux = 0,
    total_nonneg_rule_flux = 0,
    total_ocean_kill_flux = 0,
    total_sub_shelf_ice_flux = 0,
    total_sum_divQ_SIA = 0,
    total_sum_divQ_SSA = 0,
    total_surface_ice_flux = 0;
	


  PetscScalar dx = grid.dx, dy = grid.dy;
  bool do_ocean_kill = config.get_flag("ocean_kill"),
	do_mesh_refinement=config.get_flag("mesh_refinement"),
    floating_ice_killed = config.get_flag("floating_ice_killed"),
    include_bmr_in_continuity = config.get_flag("include_bmr_in_continuity"),
    compute_cumulative_climatic_mass_balance = config.get_flag("compute_cumulative_climatic_mass_balance"),
	do_part_grid = config.get_flag("part_grid"),
    do_redist = config.get_flag("part_redist");

	  if(config.get_flag("mesh_refinement")||config.get_flag("do_glmask")){
   ierr=update_glmask(); CHKERRQ(ierr);
	 }
	//update refined mesh
	 if(config.get_flag("mesh_refinement")){
	update_refined_variable(vH,vH_ref);
    update_refined_variable (acab,acab_ref);
    update_refined_variable (vbmr,vbmr_ref);
    update_refined_variable (shelfbmassflux,shelfbmassflux_ref);
	update_refined_variable (vMask,vMask_ref);
	if(do_part_grid){
	update_refined_variable(vHref,vHref_ref);
	}	
	 }

  // FIXME: use corrected cell areas (when available)
  PetscScalar factor = config.get("ice_density") * (dx * dy);

  if (surface != NULL) {
    ierr = surface->ice_surface_mass_flux(acab); CHKERRQ(ierr);
  } else { SETERRQ(grid.com, 1, "PISM ERROR: surface == NULL"); }

  if (ocean != NULL) {
    ierr = ocean->shelf_base_mass_flux(shelfbmassflux); CHKERRQ(ierr);
  } else { SETERRQ(grid.com, 2, "PISM ERROR: ocean == NULL"); }

  
  IceModelVec2S vHnew = vWork2d[0];
  ierr = vH.copy_to(vHnew); CHKERRQ(ierr);

	IceModelVec2S  *vHnew_ref,
							*vHnew_ptr;
 PetscScalar *dx_ref,*dy_ref ;
 if(config.get_flag("mesh_refinement")){
		dx_ref=&grid_refined->dx;
		dy_ref=&grid_refined->dy;
	 
	 vHnew_ref = vWork2d_ref[0];
	ierr = vH_ref->copy_to(*vHnew_ref); CHKERRQ(ierr);
}
  IceModelVec2Stag *Qdiff;
  ierr = stress_balance->get_diffusive_flux(Qdiff); CHKERRQ(ierr);

  IceModelVec2V *vel_advective;
  ierr = stress_balance->get_advective_2d_velocity(vel_advective); CHKERRQ(ierr);

  ierr = vH.begin_access(); CHKERRQ(ierr);
  ierr = vbmr.begin_access(); CHKERRQ(ierr);
  ierr = Qdiff->begin_access(); CHKERRQ(ierr);
  ierr = vel_advective->begin_access(); CHKERRQ(ierr);
  ierr = acab.begin_access(); CHKERRQ(ierr);
  ierr = shelfbmassflux.begin_access(); CHKERRQ(ierr);
  ierr = vMask.begin_access();  CHKERRQ(ierr);
  ierr = vHnew.begin_access(); CHKERRQ(ierr);

IceModelVec2Stag *Qdiff_ref;
IceModelVec2V *vel_advective_ref;
 if(config.get_flag("mesh_refinement")||config.get_flag("do_glmask")){ 
ierr = vGLMask[0]->begin_access();  CHKERRQ(ierr);
 }
 if(config.get_flag("mesh_refinement")){ 
/*	 
  ierr = stress_balance_ref->get_diffusive_flux(Qdiff_ref); CHKERRQ(ierr); //TODO maybe later
  ierr = stress_balance_ref->get_advective_2d_velocity(vel_advective_ref); CHKERRQ(ierr);
   ierr = Qdiff_ref->begin_access(); CHKERRQ(ierr);
  ierr = vel_advective_ref->begin_access(); CHKERRQ(ierr);
 */
	 
	  ierr = vH_ref->begin_access(); CHKERRQ(ierr);
  ierr = vbmr_ref->begin_access(); CHKERRQ(ierr);
 
  ierr = acab_ref->begin_access(); CHKERRQ(ierr);
  ierr = shelfbmassflux_ref->begin_access(); CHKERRQ(ierr);
  ierr = vMask_ref->begin_access();  CHKERRQ(ierr);
  ierr = vHnew_ref->begin_access(); CHKERRQ(ierr);
  if(do_part_grid){
	ierr = vHref_ref->begin_access(); CHKERRQ(ierr);
	}

  MaskQuery mask_ref(*vMask_ref);
 }

  // related to PIK part_grid mechanism; see Albrecht et al 2011
  if (do_part_grid) {
    ierr = vHref.begin_access(); CHKERRQ(ierr);
    if (do_redist) {
      ierr = vHresidual.begin_access(); CHKERRQ(ierr);
      // FIXME: next line causes mass loss if max_loopcount in redistResiduals()
      //        was not sufficient to zero-out vHresidual already
      ierr = vHresidual.set(0.0); CHKERRQ(ierr);
    }
  }
  const bool dirichlet_bc = config.get_flag("ssa_dirichlet_bc");
  if (dirichlet_bc) {
    ierr = vBCMask.begin_access();  CHKERRQ(ierr);
    ierr = vBCvel.begin_access();  CHKERRQ(ierr);
  }

  if (do_ocean_kill) {
    ierr = ocean_kill_mask.begin_access(); CHKERRQ(ierr);
  }

  if (compute_cumulative_climatic_mass_balance) {
    ierr = climatic_mass_balance_cumulative.begin_access(); CHKERRQ(ierr);
  }
  MaskQuery mask(vMask);

/*
struct massContStruct mcs_ref;
struct massContStruct mcs;
mcs.acab=&acab;
mcs.include_bmr_in_continuity=include_bmr_in_continuity;
mcs.floating_ice_killed=floating_ice_killed;
mcs.compute_cumulative_climatic_mass_balance=compute_cumulative_climatic_mass_balance;
*/
 bool no_refinement_iteration=true;
 PetscInt *i_ptr,
			 *j_ptr;
PetscScalar *dx_ptr,
				  *dy_ptr;
PetscInt weighting;
PetscPrintf(grid.com,"DURCHLAUF ");
PetscPrintf(grid.com,"Hoehe: %f  GL:%f \n",vH(200,3),(*vGLMask[0])(200,3));
  for (PetscInt i = grid.xs; i < grid.xs + grid.xm; ++i) {
    for (PetscInt j = grid.ys; j < grid.ys + grid.ym; ++j) {
	
		PetscInt i_ref = refinement*i,
					j_ref = refinement*j;
		
		if(do_mesh_refinement)	{
			if((*vGLMask[0])(i,j)==1){
			//PetscPrintf(grid.com,"GLPos:%d %d\n",i,j);
			no_refinement_iteration=false;
			i_ptr=   &i_ref;		j_ptr=	&j_ref;
			dx_ptr=&dx;
			dy_ptr=&dy;
			vH_ptr=vH_ref;
			vHnew_ptr=vHnew_ref;
			vHref_ptr=vHref_ref;
			weighting=refinement*refinement;
			}
			else{ 
			no_refinement_iteration=true;
			i_ptr=   &i;
			j_ptr=	&j;
			dx_ptr=&dx;
			dy_ptr=&dy;
			vH_ptr=&vH;	
			vHnew_ptr=&vHnew;
			vHref_ptr=&vHref;
			weighting=1;
			}
		}
		else{
		no_refinement_iteration=true;
			i_ptr=   &i;
			j_ptr=	&j;
			dx_ptr=&dx;
			dy_ptr=&dy;
			vH_ptr=&vH;	
			vHnew_ptr=&vHnew;
			vHref_ptr=&vHref;
			weighting=1;   
		}
		
		//if (!no_refinement_iteration){PetscPrintf(grid.com,"\n ");} //TODO test
		 for (; i_ref < (1+i)*refinement; ++i_ref) {
		for (; j_ref < (1+j)*refinement; ++j_ref) {
		//if (!no_refinement_iteration){PetscPrintf(grid.com,"Iteration: %d, %d Hoehe:%f ",i_ref,j_ref,(*vH_ref)(i_ref,j_ref));} //TODO test
		//if (!no_refinement_iteration){PetscPrintf(grid.com,"Iteration:\nHoehe neu:%f \n ",(*vHnew_ptr)(*i_ptr, *j_ptr));} //TODO test
      // Divergence terms:

	/*if (*dx_ref!=dx||*dy_ref!=dy ){//TODO test
			PetscPrintf(grid.com,"ACHTUNG!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! %f %f %f %f  %f\n\n\n",*dx_ref,dx,*dy_ref,dy,dx );
		}*/

	
double divQ_SIA = 0.0, divQ_SSA = 0.0;

      // Source terms:
      double
        surface_mass_balance = acab(i, j),
        meltrate_grounded = 0.0,
        meltrate_floating = 0.0,
        H_to_Href_flux    = 0.0,
        Href_to_H_flux    = 0.0,
        ocean_kill_flux   = 0.0,
        float_kill_flux   = 0.0,
        nonneg_rule_flux  = 0.0;

      if (include_bmr_in_continuity) {
        meltrate_floating = shelfbmassflux(i, j);
        meltrate_grounded = vbmr(i, j);
      }

      planeStar<PetscScalar> Q, v;
      cell_interface_fluxes(dirichlet_bc, i, j,
                            vel_advective->star(i, j), Qdiff->star(i, j),
                            v, Q);

      // Compute divergence terms:
      {
        // Staggered grid Div(Q) for diffusive non-sliding SIA deformation part:
        // divQ_SIA = - D grad h
        divQ_SIA = (Q.e - Q.w) / dx + (Q.n - Q.s) / dy;
		//if (no_refinement_iteration){PetscPrintf(grid.com,"divQ B %f\n ",divQ_SIA);} //TODO test 
        // Plug flow part (i.e. basal sliding; from SSA): upwind by staggered grid
        // PIK method;  this is   \nabla \cdot [(u, v) H]
        divQ_SSA += ( v.e * (v.e > 0 ? (*vH_ptr)(*i_ptr, *j_ptr) : (*vH_ptr)(*i_ptr+1, *j_ptr))
                      - v.w * (v.w > 0 ? (*vH_ptr)(*i_ptr-1, *j_ptr) : (*vH_ptr)(*i_ptr, *j_ptr)) ) / (*dx_ptr); 
		 
        divQ_SSA += ( v.n * (v.n > 0 ? (*vH_ptr)(*i_ptr, *j_ptr) : (*vH_ptr)(*i_ptr, *j_ptr+1))
                      - v.s * (v.s > 0 ? (*vH_ptr)(*i_ptr, *j_ptr-1) : (*vH_ptr)(*i_ptr, *j_ptr)) ) / (*dy_ptr);
		
      }

      // Set source terms

      // Basal melt:
      if (mask.grounded(i,j)) {
        meltrate_floating = 0.0;
      } else {
        meltrate_grounded = 0.0;
      }

      if (mask.ice_free_ocean(i, j)) {
        // Decide whether to apply Albrecht et al 2011 subgrid-scale
        // parameterization
        if (do_part_grid && mask.next_to_floating_ice(i, j)) {

          // Add the flow contribution to this partially filled cell.
          H_to_Href_flux = -(divQ_SSA + divQ_SIA) * dt;
          (*vHref_ptr)(*i_ptr, *j_ptr) += H_to_Href_flux;
          if ((*vHref_ptr)(*i_ptr, *j_ptr) < 0) {
            ierr = verbPrintf(3, grid.com,
                              "PISM WARNING: negative Href at (%d,%d)\n",
                              i, j); CHKERRQ(ierr);

            // Note: this adds mass!
            nonneg_rule_flux += (*vHref_ptr)(*i_ptr, *j_ptr);
			 
           (*vHref_ptr)(*i_ptr, *j_ptr) = 0;
			  
          }

          PetscReal H_average = get_average_thickness(do_redist,
                                                      vMask.int_star(i, j),
                                                      vH_ptr->star(*i_ptr, *j_ptr)),
            coverage_ratio = (*vH_ptr)(*i_ptr, *j_ptr) / H_average;

          if (coverage_ratio >= 1.0) {
            // A partially filled grid cell is now considered to be full.
            if (do_redist)
              vHresidual(i, j) = (*vHref_ptr)(*i_ptr, *j_ptr) - H_average; // residual ice thickness
			  
            (*vHref_ptr)(*i_ptr, *j_ptr) = 0.0;

            Href_to_H_flux = H_average;

            // A cell that became "full" experiences both SMB and basal melt.
          } else {
            surface_mass_balance = 0.0;
            meltrate_floating    = 0.0;
          }

          // In this case the SSA flux goes into the Href variable and does not
          // directly contribute to ice thickness at this location
          proc_sum_divQ_SIA += - divQ_SIA;
          proc_sum_divQ_SSA += - divQ_SSA;
          divQ_SIA = divQ_SSA = 0;
        }  else { // end of "if (part_grid...)

          // Standard ice-free ocean case:
          surface_mass_balance = 0.0;
          meltrate_floating    = 0.0;
        }
      } // end of "if (ice_free_ocean)"


      // Dirichlet BC case (should go last to override previous settings):
      if (dirichlet_bc && vBCMask.as_int(i,j) == 1) {
        surface_mass_balance = 0.0;
        meltrate_grounded    = 0.0;
        meltrate_floating    = 0.0;
        Href_to_H_flux       = 0.0;
        divQ_SIA             = 0.0;
        divQ_SSA             = 0.0;
      }

			
 //if ((*vGLMask[0])(i,j)==1){PetscPrintf(grid.com,"Werte:: %f, %f, %f,H: %f \n ",1000*surface_mass_balance,
   //                                       1000*divQ_SIA, 1000*divQ_SSA,(*vHnew_ptr)(*i_ptr, *j_ptr));}
      (*vHnew_ptr)(*i_ptr, *j_ptr) += (dt * (surface_mass_balance // accumulation/ablation
                            - meltrate_grounded // basal melt rate (grounded)
                            - meltrate_floating // sub-shelf melt rate
                            - (divQ_SIA + divQ_SSA)) // flux divergence
                      + Href_to_H_flux); // corresponds to a cell becoming "full"

      if ((*vHnew_ptr)(*i_ptr, *j_ptr) < 0.0) {
        nonneg_rule_flux += -(*vHnew_ptr)(*i_ptr, *j_ptr); //TODO flux?

        // this has to go *after* accounting above!
        (*vHnew_ptr)(*i_ptr, *j_ptr) = 0.0;
      }

      // "Calving" mechanisms

      // FIXME: we should update the mask first and then do calving. These
      // should go into separate methods (and it's OK to loop over the grid again).
      {
        // the following conditionals, both -ocean_kill and -float_kill, are also applied in
        //   IceModel::computeMax2DSlidingSpeed() when determining CFL

        // force zero thickness at points which were originally ocean (if "-ocean_kill");
        //   this is calving at original calving front location
        if ( do_ocean_kill && ocean_kill_mask.as_int(i, j) == 1) {
          ocean_kill_flux = -(*vHnew_ptr)(*i_ptr, *j_ptr);

          // this has to go *after* accounting above!
          (*vHnew_ptr)(*i_ptr, *j_ptr) = 0.0;
        }

        // force zero thickness at points which are floating (if "-float_kill");
        //   this is calving at grounding line
        if ( floating_ice_killed && mask.ocean(i, j) ) { // FIXME: *was* ocean???
          float_kill_flux = -(*vHnew_ptr)(*i_ptr, *j_ptr);

          // this has to go *after* accounting above!
          (*vHnew_ptr)(*i_ptr, *j_ptr) = 0.0;
        }
      }

      // Track cumulative surface mass balance. Note that this keeps track of
      // cumulative acab at all the grid cells (including ice-free cells).
      if (compute_cumulative_climatic_mass_balance) {
        climatic_mass_balance_cumulative(i, j) += acab(i, j) * dt;
      }

      // accounting:
      { 
		
        proc_grounded_basal_ice_flux -= meltrate_grounded/(refinement*refinement);
        proc_sub_shelf_ice_flux      -= meltrate_floating/weighting;
        proc_surface_ice_flux        += surface_mass_balance/weighting;
        proc_float_kill_flux         += float_kill_flux/weighting;
        proc_ocean_kill_flux         += ocean_kill_flux/weighting;
        proc_nonneg_rule_flux        += nonneg_rule_flux/weighting;
        proc_sum_divQ_SIA += - divQ_SIA/weighting;
        proc_sum_divQ_SSA += - divQ_SSA/weighting;
        proc_H_to_Href_flux -= H_to_Href_flux/weighting;
        proc_Href_to_H_flux += Href_to_H_flux/weighting;
      }
			if (no_refinement_iteration) break;
			} //end of j_ref loop
		if (no_refinement_iteration) break;
		j_ref = refinement*j;
		} //end of i_ref loop	
    } // end of the inner (j) for loop
  } // end of the outer (i) for loop

  ierr = vbmr.end_access(); CHKERRQ(ierr);
  ierr = vMask.end_access(); CHKERRQ(ierr);
  ierr = Qdiff->end_access(); CHKERRQ(ierr);
  ierr = vel_advective->end_access(); CHKERRQ(ierr);
  ierr = acab.end_access(); CHKERRQ(ierr);
  ierr = shelfbmassflux.end_access(); CHKERRQ(ierr);
  ierr = vH.end_access(); CHKERRQ(ierr);
  ierr = vHnew.end_access(); CHKERRQ(ierr);
    
 if(config.get_flag("mesh_refinement")||config.get_flag("do_glmask")){ 
ierr = vGLMask[0]->end_access();  CHKERRQ(ierr);
 }    
if(config.get_flag("mesh_refinement")){
ierr = vbmr_ref->end_access(); CHKERRQ(ierr);
  ierr = vMask_ref->end_access(); CHKERRQ(ierr);
	/*
  ierr = Qdiff_ref->end_access(); CHKERRQ(ierr);
  ierr = vel_advective_ref->end_access(); CHKERRQ(ierr);
	*/
  ierr = acab_ref->end_access(); CHKERRQ(ierr);
  ierr = shelfbmassflux_ref->end_access(); CHKERRQ(ierr);
  ierr = vH_ref->end_access(); CHKERRQ(ierr);
  ierr = vHnew_ref->end_access(); CHKERRQ(ierr);
  if(do_part_grid){
	ierr = vHref_ref->end_access(); CHKERRQ(ierr);
	//update_unrefined_variable(vHref, vHref_ref);
	}
	
 update_unrefined_variable(vH, vH_ref);
update_unrefined_variable(vHnew,vHnew_ref);
}	

  if (compute_cumulative_climatic_mass_balance) {
    ierr = climatic_mass_balance_cumulative.end_access(); CHKERRQ(ierr);
  }

  if (do_part_grid) {
    ierr = vHref.end_access(); CHKERRQ(ierr);
    if (do_redist) {
      ierr = vHresidual.end_access(); CHKERRQ(ierr);
    }
  }

  if (dirichlet_bc) {
    ierr = vBCMask.end_access();  CHKERRQ(ierr);
    ierr = vBCvel.end_access();  CHKERRQ(ierr);
  }

  if (do_ocean_kill) {
    ierr = ocean_kill_mask.end_access(); CHKERRQ(ierr);
  }

  // flux accounting
  {
    ierr = PISMGlobalSum(&proc_grounded_basal_ice_flux, &total_grounded_basal_ice_flux, grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_float_kill_flux,    &total_float_kill_flux,    grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_nonneg_rule_flux,   &total_nonneg_rule_flux,   grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_ocean_kill_flux,    &total_ocean_kill_flux,    grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_sub_shelf_ice_flux, &total_sub_shelf_ice_flux, grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_surface_ice_flux,   &total_surface_ice_flux,   grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_sum_divQ_SIA,       &total_sum_divQ_SIA,       grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_sum_divQ_SSA,       &total_sum_divQ_SSA,       grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_Href_to_H_flux,     &total_Href_to_H_flux,     grid.com); CHKERRQ(ierr);
    ierr = PISMGlobalSum(&proc_H_to_Href_flux,     &total_H_to_Href_flux,     grid.com); CHKERRQ(ierr);

    // these are computed using accumulation/ablation or melt rates, so we need
    // to multiply by dt
    cumulative_grounded_basal_ice_flux += total_grounded_basal_ice_flux * factor * dt;
    cumulative_sub_shelf_ice_flux += total_sub_shelf_ice_flux * factor * dt;
    cumulative_surface_ice_flux   += total_surface_ice_flux   * factor * dt;
    cumulative_sum_divQ_SIA       += total_sum_divQ_SIA       * factor * dt;
    cumulative_sum_divQ_SSA       += total_sum_divQ_SSA       * factor * dt;
    // these are computed using ice thickness and are "cumulative" already
    cumulative_float_kill_flux    += total_float_kill_flux    * factor;
    cumulative_nonneg_rule_flux   += total_nonneg_rule_flux   * factor;
    cumulative_ocean_kill_flux    += total_ocean_kill_flux    * factor;
    cumulative_Href_to_H_flux     += total_Href_to_H_flux     * factor;
    cumulative_H_to_Href_flux     += total_H_to_Href_flux     * factor;
  }

  // finally copy vHnew into vH and communicate ghosted values
  ierr = vHnew.beginGhostComm(vH); CHKERRQ(ierr);
  ierr = vHnew.endGhostComm(vH); CHKERRQ(ierr);
  if(do_mesh_refinement){
  ierr = vHnew_ref->beginGhostComm(*vH_ref); CHKERRQ(ierr);
  ierr = vHnew_ref->endGhostComm(*vH_ref); CHKERRQ(ierr);
  }


  // the following calls are new routines adopted from PISM-PIK. The place and
  // order is not clear yet!

  // There is no reporting of single ice fluxes yet in comparison to total ice
  // thickness change.

  // distribute residual ice mass if desired
  if (do_redist) {
    ierr = redistResiduals(); CHKERRQ(ierr);
  }

  // FIXME: calving should be applied *before* the redistribution part!
  if (config.get_flag("do_eigen_calving") && config.get_flag("use_ssa_velocity")) {
     bool dteigencalving = config.get_flag("cfl_eigencalving");
     if (!dteigencalving){ // calculation of strain rates has been done in iMadaptive.cc already
       ierr = stress_balance->get_principal_strain_rates(vPrinStrain1, vPrinStrain2); CHKERRQ(ierr);
     }
     ierr = eigenCalving(); CHKERRQ(ierr);
  }

  if (config.get_flag("do_thickness_calving") && config.get_flag("part_grid")) {
    ierr = calvingAtThickness(); CHKERRQ(ierr);
  }

  // Check if the ice thickness exceeded the height of the computational box
  // and extend the grid if necessary:
  ierr = check_maximum_thickness(); CHKERRQ(ierr);

  return 0;
}

