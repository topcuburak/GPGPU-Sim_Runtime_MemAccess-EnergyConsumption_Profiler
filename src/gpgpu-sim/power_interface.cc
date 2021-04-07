// Copyright (c) 2009-2011, Tor M. Aamodt, Ahmed El-Shafiey, Tayler Hetherington
// The University of British Columbia
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution. Neither the name of
// The University of British Columbia nor the names of its contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "power_interface.h"

enum hw_perf_t {
  HW_BENCH_NAME=0,
  HW_L1_RH,
  HW_L1_RM,
  HW_L1_WH,
  HW_L1_WM,
  HW_CC,
  HW_SHRD,
  HW_DRAM_RD,
  HW_DRAM_WR,
  HW_L2_RH,
  HW_L2_RM,
  HW_L2_WH,
  HW_L2_WM,
  HW_NOC,
  HW_PIPE_DUTY,
  HW_NUM_SM_IDLE
};

void init_mcpat(const gpgpu_sim_config &config,
                class gpgpu_sim_wrapper *wrapper, unsigned stat_sample_freq,
                unsigned tot_inst, unsigned inst) {
  wrapper->init_mcpat(
      config.g_power_config_name, config.g_power_filename,
      config.g_power_trace_filename, config.g_metric_trace_filename,
      config.g_steady_state_tracking_filename,
      config.g_power_simulation_enabled, config.g_power_trace_enabled,
      config.g_steady_power_levels_enabled, config.g_power_per_cycle_dump,
      config.gpu_steady_power_deviation, config.gpu_steady_min_period,
      config.g_power_trace_zlevel, tot_inst + inst, stat_sample_freq,  config.g_power_simulation_mode);
}

void mcpat_cycle(const gpgpu_sim_config &config,
                 const shader_core_config *shdr_config,
                 class gpgpu_sim_wrapper *wrapper,
                 class power_stat_t *power_stats, unsigned stat_sample_freq,
                 unsigned tot_cycle, unsigned cycle, unsigned tot_inst,
                 unsigned inst) {
  static bool mcpat_init = true;

  if (mcpat_init) {  // If first cycle, don't have any power numbers yet
    mcpat_init = false;
    return;
  }


  if ((tot_cycle + cycle) % stat_sample_freq == 0) {
    wrapper->set_inst_power(
        shdr_config->gpgpu_clock_gated_lanes, stat_sample_freq,
        stat_sample_freq, power_stats->get_total_inst(0),
        power_stats->get_total_int_inst(0), power_stats->get_total_fp_inst(0),
        power_stats->get_l1d_read_accesses(),
        power_stats->get_l1d_write_accesses(),
        power_stats->get_committed_inst(0));

    // Single RF for both int and fp ops
    wrapper->set_regfile_power(power_stats->get_regfile_reads(0),
                               power_stats->get_regfile_writes(0),
                               power_stats->get_non_regfile_operands(0));

    // Instruction cache stats
    wrapper->set_icache_power(power_stats->get_inst_c_hits(0),
                              power_stats->get_inst_c_misses(0));

    // Constant Cache, shared memory, texture cache
    wrapper->set_ccache_power(power_stats->get_const_accessess(), 0); //assuming all HITS in constant cache for now
    wrapper->set_tcache_power(power_stats->get_texture_c_hits(),
                              power_stats->get_texture_c_misses());
    wrapper->set_shrd_mem_power(power_stats->get_shmem_read_access());

    wrapper->set_l1cache_power(
        power_stats->get_l1d_read_hits(), power_stats->get_l1d_read_misses(),
        power_stats->get_l1d_write_hits(), power_stats->get_l1d_write_misses());

    wrapper->set_l2cache_power(
        power_stats->get_l2_read_hits(), power_stats->get_l2_read_misses(),
        power_stats->get_l2_write_hits(), power_stats->get_l2_write_misses());

    float active_sms = (*power_stats->m_active_sms) / stat_sample_freq;
    float num_cores = shdr_config->num_shader();
    float num_idle_core = num_cores - active_sms;
    wrapper->set_num_cores(num_cores);
    wrapper->set_idle_core_power(num_idle_core);

    // pipeline power - pipeline_duty_cycle *= percent_active_sms;
    float pipeline_duty_cycle =
        ((*power_stats->m_average_pipeline_duty_cycle / (stat_sample_freq)) <
         0.8)
            ? ((*power_stats->m_average_pipeline_duty_cycle) / stat_sample_freq)
            : 0.8;
    wrapper->set_duty_cycle_power(pipeline_duty_cycle);

    // Memory Controller
    wrapper->set_mem_ctrl_power(power_stats->get_dram_rd(),
                                power_stats->get_dram_wr(),
                                power_stats->get_dram_pre());

    // Execution pipeline accesses
    // FPU (SP) accesses, Integer ALU (not present in Tesla), Sfu accesses

    wrapper->set_int_accesses(power_stats->get_ialu_accessess(0), 
                              power_stats->get_intmul24_accessess(0), 
                              power_stats->get_intmul32_accessess(0), 
                              power_stats->get_intmul_accessess(0), 
                              power_stats->get_intdiv_accessess(0));

    wrapper->set_dp_accesses(power_stats->get_dp_accessess(0), 
                              power_stats->get_dpmul_accessess(0), 
                              power_stats->get_dpdiv_accessess(0));

    wrapper->set_fp_accesses(power_stats->get_fp_accessess(0), 
                            power_stats->get_fpmul_accessess(0), 
                            power_stats->get_fpdiv_accessess(0));

    wrapper->set_trans_accesses(power_stats->get_sqrt_accessess(0), 
                                power_stats->get_log_accessess(0), 
                                power_stats->get_sin_accessess(0), 
                                power_stats->get_exp_accessess(0));

    wrapper->set_tensor_accesses(power_stats->get_tensor_accessess(0));

    wrapper->set_tex_accesses(power_stats->get_tex_accessess(0));

    wrapper->set_exec_unit_power(power_stats->get_tot_fpu_accessess(0),
                                 power_stats->get_ialu_accessess(0),
                                 power_stats->get_tot_sfu_accessess(0));

    wrapper->set_avg_active_threads(power_stats->get_active_threads());

    // Average active lanes for sp and sfu pipelines
    float avg_sp_active_lanes =
        (power_stats->get_sp_active_lanes()) / stat_sample_freq;
    float avg_sfu_active_lanes =
        (power_stats->get_sfu_active_lanes()) / stat_sample_freq;
    if(avg_sp_active_lanes >32.0 )
      avg_sp_active_lanes = 32.0;
    if(avg_sfu_active_lanes >32.0 )
      avg_sfu_active_lanes = 32.0;
    assert(avg_sp_active_lanes <= 32);
    assert(avg_sfu_active_lanes <= 32);
    wrapper->set_active_lanes_power(avg_sp_active_lanes, avg_sfu_active_lanes);

    double n_icnt_simt_to_mem =
        (double)
            power_stats->get_icnt_simt_to_mem();  // # flits from SIMT clusters
                                                  // to memory partitions
    double n_icnt_mem_to_simt =
        (double)
            power_stats->get_icnt_mem_to_simt();  // # flits from memory
                                                  // partitions to SIMT clusters
    wrapper->set_NoC_power(n_icnt_mem_to_simt + n_icnt_simt_to_mem);  // Number of flits traversing the interconnect

    wrapper->compute();

    wrapper->update_components_power();
    wrapper->print_trace_files();
    power_stats->save_stats();

    wrapper->detect_print_steady_state(0, tot_inst + inst);

    wrapper->power_metrics_calculations();

    wrapper->dump();
  }
  // wrapper->close_files();
}

void mcpat_reset_perf_count(class gpgpu_sim_wrapper *wrapper) {
  wrapper->reset_counters();
}


void calculate_hw_mcpat(const gpgpu_sim_config &config,
                 const shader_core_config *shdr_config,
                 class gpgpu_sim_wrapper *wrapper,
                 class power_stat_t *power_stats, unsigned stat_sample_freq,
                 unsigned tot_cycle, unsigned cycle, unsigned tot_inst,
                 unsigned inst, int power_simulation_mode, char* hwpowerfile, char* kernelname, std::string executed_kernelname){


	/* Finding the correct kernel name for HW data CSV file  */
	char* current_kernelname;
	if(kernelname == "b+tree-rodinia-3.1"){
		if(executed_kernelname == "findRangeK")
			current_kernelname = "b+tree-rodinia-3.1_k1";
		else
			current_kernelname = "b+tree-rodinia-3.1_k2";
	} 

	else if(kernelname == "backprop-rodinia-3.1"){
		if(executed_kernelname == "_Z22bpnn_layerforward_CUDAPfS_S_S_ii")
			current_kernelname = "backprop-rodinia-3.1_k1";
		else
			current_kernelname = "backprop-rodinia-3.1_k2";
	} 	

	else if(kernelname == "dct8x8"){
		if(executed_kernelname == "_Z14CUDAkernel1DCTPfiiiy")
			current_kernelname = "dct8x8_k1";
		else
			current_kernelname = "dct8x8_k2";
	} 

	else if(kernelname == "fastWalshTransform"){
		if(executed_kernelname == "_Z15fwtBatch2KernelPfS_i")
			current_kernelname = "fastWalshTransform_k1";
		else
			current_kernelname = "fastWalshTransform_k2";
	} 

	else if(kernelname == "mergeSort"){
		if(executed_kernelname == "_Z30mergeElementaryIntervalsKernelILj1EEvPjS0_S0_S0_S0_S0_jj")
			current_kernelname = "mergeSort_k1";
		else
			current_kernelname = "mergeSort_k2";
	} 

	else if(kernelname == "quasirandomGenerator"){
		if(executed_kernelname == "_Z26quasirandomGeneratorKernelPfjj")
			current_kernelname = "quasirandomGenerator_k1";
		else
			current_kernelname = "quasirandomGenerator_k2";
	} 
	else
		current_kernelname = kernelname;



	/* Reading HW data from CSV file */
	fstream hw_file;
	hw_file.open(hwpowerfile, ios::in);
	vector<string> hw_data;
    string line, word, temp;
    bool kernel_found = false;
    while(!hw_file.eof()){
    	hw_data.clear();
    	getline(hw_file, line);
    	stringstream s(line);
    	while (getline(s,word,',')){
    		hw_data.push_back(word);
    	}
	    if(hw_data[HW_BENCH_NAME] == current_kernelname){
	    	kernel_found = true;
	    	break;
	    }
	}
	assert(kernel_found);
	wrapper->init_mcpat_hw_mode(cycle); //total simulated cycles for current kernel

    wrapper->set_inst_power(
        shdr_config->gpgpu_clock_gated_lanes, cycle,
        cycle, power_stats->get_total_inst(1),
        power_stats->get_total_int_inst(1), power_stats->get_total_fp_inst(1),
        std::stod(hw_data[HW_L1_RH]) + std::stod(hw_data[HW_L1_RM]),
        std::stod(hw_data[HW_L1_WH]) + std::stod(hw_data[HW_L1_WM]),
        power_stats->get_committed_inst(1));

    // Single RF for both int and fp ops
    wrapper->set_regfile_power(power_stats->get_regfile_reads(1),
                               power_stats->get_regfile_writes(1),
                               power_stats->get_non_regfile_operands(1));

    // Instruction cache stats
    wrapper->set_icache_power(power_stats->get_inst_c_hits(1),
                              power_stats->get_inst_c_misses(1));

    // Constant Cache, shared memory, texture cache
    wrapper->set_ccache_power(std::stod(hw_data[HW_CC]), 0); //assuming all HITS in constant cache for now
    // wrapper->set_tcache_power(power_stats->get_texture_c_hits(),
    //                           power_stats->get_texture_c_misses());
    wrapper->set_shrd_mem_power(std::stod(hw_data[HW_SHRD]));

    wrapper->set_l1cache_power(
        std::stod(hw_data[HW_L1_RH]),  std::stod(hw_data[HW_L1_RM]),
        std::stod(hw_data[HW_L1_WH]),  std::stod(hw_data[HW_L1_WM]));

    wrapper->set_l2cache_power(
       std::stod(hw_data[HW_L2_RH]),  std::stod(hw_data[HW_L2_RM]),
        std::stod(hw_data[HW_L2_WH]),  std::stod(hw_data[HW_L2_WM]));

    // float active_sms = (*power_stats->m_active_sms) / stat_sample_freq;
    float num_cores = shdr_config->num_shader();
    // float num_idle_core = num_cores - active_sms;
    wrapper->set_num_cores(num_cores);
    wrapper->set_idle_core_power(std::stod(hw_data[HW_NUM_SM_IDLE]));

    // pipeline power - pipeline_duty_cycle *= percent_active_sms;
    // float pipeline_duty_cycle =
    //     ((*power_stats->m_average_pipeline_duty_cycle / (stat_sample_freq)) <
    //      0.8)
    //         ? ((*power_stats->m_average_pipeline_duty_cycle) / stat_sample_freq)
    //         : 0.8;
    wrapper->set_duty_cycle_power(std::stod(hw_data[HW_PIPE_DUTY]));

    // Memory Controller
    if(power_simulation_mode == 1)
    	wrapper->set_mem_ctrl_power(std::stod(hw_data[HW_DRAM_RD]),
                                std::stod(hw_data[HW_DRAM_WR]),
                                0);
    else if(power_simulation_mode == 2)
    	wrapper->set_mem_ctrl_power(std::stod(hw_data[HW_DRAM_RD]),
                                std::stod(hw_data[HW_DRAM_WR]),
                                power_stats->get_dram_pre());
    else
    	assert(power_simulation_mode>0 && power_simulation_mode<3);
    // Execution pipeline accesses
    // FPU (SP) accesses, Integer ALU (not present in Tesla), Sfu accesses

    wrapper->set_int_accesses(power_stats->get_ialu_accessess(1), 
                              power_stats->get_intmul24_accessess(1), 
                              power_stats->get_intmul32_accessess(1), 
                              power_stats->get_intmul_accessess(1), 
                              power_stats->get_intdiv_accessess(1));

    wrapper->set_dp_accesses(power_stats->get_dp_accessess(1), 
                              power_stats->get_dpmul_accessess(1), 
                              power_stats->get_dpdiv_accessess(1));

    wrapper->set_fp_accesses(power_stats->get_fp_accessess(1), 
                            power_stats->get_fpmul_accessess(1), 
                            power_stats->get_fpdiv_accessess(1));

    wrapper->set_trans_accesses(power_stats->get_sqrt_accessess(1), 
                                power_stats->get_log_accessess(1), 
                                power_stats->get_sin_accessess(1), 
                                power_stats->get_exp_accessess(1));

    wrapper->set_tensor_accesses(power_stats->get_tensor_accessess(1));

    wrapper->set_tex_accesses(power_stats->get_tex_accessess(1));

    wrapper->set_exec_unit_power(power_stats->get_tot_fpu_accessess(1),
                                 power_stats->get_ialu_accessess(1),
                                 power_stats->get_tot_sfu_accessess(1));

    wrapper->set_avg_active_threads(power_stats->get_active_threads());

    // Average active lanes for sp and sfu pipelines
    float avg_sp_active_lanes =
        (power_stats->get_sp_active_lanes()) / stat_sample_freq;
    float avg_sfu_active_lanes =
        (power_stats->get_sfu_active_lanes()) / stat_sample_freq;
    if(avg_sp_active_lanes >32.0 )
      avg_sp_active_lanes = 32.0;
    if(avg_sfu_active_lanes >32.0 )
      avg_sfu_active_lanes = 32.0;
    assert(avg_sp_active_lanes <= 32);
    assert(avg_sfu_active_lanes <= 32);
    wrapper->set_active_lanes_power(avg_sp_active_lanes, avg_sfu_active_lanes);

    // double n_icnt_simt_to_mem =
    //     (double)
    //         power_stats->get_icnt_simt_to_mem();  // # flits from SIMT clusters
    //                                               // to memory partitions
    // double n_icnt_mem_to_simt =
    //     (double)
    //         power_stats->get_icnt_mem_to_simt();  // # flits from memory
    //                                               // partitions to SIMT clusters
    wrapper->set_NoC_power(std::stod(hw_data[HW_NOC]));  // Number of flits traversing the interconnect

    wrapper->compute();

    wrapper->update_components_power();

    wrapper->power_metrics_calculations();

    wrapper->dump();

}