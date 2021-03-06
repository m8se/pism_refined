# Add the -config to all PISM test runs (necessary because we want to use the
# config file from *this* build, not the one that might be present in the
# install location).
file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/.petscrc "-config ${PROJECT_BINARY_DIR}/pism_config.nc")

# Copy nccmp.py into the build directory:
configure_file (${PROJECT_SOURCE_DIR}/util/nccmp.py ${PROJECT_BINARY_DIR}/nccmp.py COPYONLY)

# Set the location of PISM regression test scripts
set(PISM_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# A macro simplifying test creation:
macro(pism_test name script)
  add_test("${name}:${script}" ${PISM_TEST_DIR}/${script} ${PROJECT_BINARY_DIR} ${MPIEXEC} ${PROJECT_SOURCE_DIR})
endmacro()

# Tests themselves:
pism_test (pism_testing_tools test_00.sh)

pism_test (pismr_exact_restartability_SIA_only test_01.sh)

pism_test (pismv_processor_independence test_02.sh)

pism_test (pismr_zero_length_run test_03.sh)

pism_test (pismr_regridding_during_bootstrapping test_04.sh)

pism_test (pismr_bootstrap_variable_order test_05.sh)

pism_test (pismr_bootstrap_variable_ranges test_06.sh)

pism_test (regridding_via_boot_file_and_regrid_file test_07.sh)

pism_test (regridding_in_vertical_direction test_08.sh)

pism_test (pismr_regridding_variable_order test_09.sh)

pism_test (pismr_regridding_processor_independence test_10.sh)

pism_test (automatic_vertical_grid_extension test_11.sh)

pism_test (SIA_mass_conservation test_12.sh)

pism_test (temperature_continuity_base_polythermal temp_continuity.py)

pism_test (enthalpy_symmetry_near_base test_13.sh)

pism_test (verif_test_E_regress_SIA_sliding test_14.sh)

pism_test (verif_test_C_regress_SIA_time_SMB test_15.sh)

pism_test (verif_test_L_regress_SIA_nonflat_bed test_16.sh)

pism_test (verif_test_G_regress_thermo_SIA_time_SMB test_17.sh)

pism_test (verif_test_I_SSAFD_regress_SSA_plastic ssa/ssa_testi_fd.sh)

pism_test (verif_test_I_SSAFEM_regress_SSA_plastic ssa/ssa_testi_fem.sh)

pism_test (verif_test_J_SSAFD_regress_linear_SSA_floating ssa/ssa_testj_fd.sh)

pism_test (verif_test_J_SSAFEM_regress_linear_SSA_floating ssa/ssa_testj_fem.sh)

pism_test (verif_SSAFEM_regress_linear_flow ssa/ssafem_test_linear.sh)

pism_test (verif_SSAFEM_regress_plug_flow ssa/ssafem_test_plug.sh)

pism_test (verif_test_K_regress_cold_ice_bedrock_thermal test_18.sh)

pism_test (GPBLD_flow_law_regress test_20.sh)

pism_test (Paterson-Budd_flow_law_regress test_21.sh)

pism_test (Schoof_2003_bed_roughness_SIA_regress test_22.sh)

pism_test (comparing_restart_i_vs_boot_file_and_regrid_file test_23.sh)

pism_test (Hooke_flow_law_regress test_24.sh)

pism_test (GK_flow_law_regress test_25.sh)

pism_test (cold_part_of_Paterson-Budd_flow_law_regress test_26.sh)

pism_test (verif_PISMBedThermalUnit_test_K_regress btu_regression.sh)

pism_test (runtime_viewers test_27.sh)

pism_test (bootstrapping_incomplete_input test_28.sh)

