
/* Copyright 2009, 2010 Brendan Tauras */
/* run_test.cpp is part of FlashSim. */

/* FlashSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version. */

/* FlashSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License
 * along with FlashSim.  If not, see <http://www.gnu.org/licenses/>. */

/****************************************************************************/

/* Basic test driver
 * Brendan Tauras 2009-11-02
 *
 * driver to create and run a very basic test of writes then reads */

#include "ssd.h"
#include <unistd.h>   // chdir
#include <sys/stat.h> // mkdir
#include <sstream>

using namespace ssd;

vector<Thread*>  random_writes_reads_experiment(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 0;
	SCHEDULING_SCHEME = 2;
	long num_IOs = numeric_limits<int>::max();
	Thread* t1 = new Asynchronous_Sequential_Thread(0, highest_lba, 1, WRITE, IO_submission_rate, 1);
	t1->add_follow_up_thread(new Asynchronous_Random_Thread_Reader_Writer(0, highest_lba, num_IOs, 2, 1));
	vector<Thread*> threads;
	threads.push_back(t1);
	return threads;
}

int main()
{
	string exp_folder  = "exp_copyback_map/";
	mkdir(exp_folder.c_str(), 0755);

	load_config();

	/*
	 * sequential_writes_lazy_gc
	 * sequential_writes_greedy_gc
	 * random_writes_greedy_gc
	 * random_writes_lazy_gc
	 */

	MAX_SSD_QUEUE_SIZE = 16;

	SSD_SIZE = 4;
	PACKAGE_SIZE = 2;
	DIE_SIZE = 1;
	PLANE_SIZE = 512;
	BLOCK_SIZE = 32;

	PAGE_READ_DELAY = 5;
	PAGE_WRITE_DELAY = 20;
	BUS_CTRL_DELAY = 1;
	BUS_DATA_DELAY = 8;
	BLOCK_ERASE_DELAY = 150;

	MAX_ITEMS_IN_COPY_BACK_MAP = NUMBER_OF_ADDRESSABLE_BLOCKS() * BLOCK_SIZE;

	printf("Number of addressable blocks: %d\n", NUMBER_OF_ADDRESSABLE_BLOCKS() * BLOCK_SIZE);

	int IO_limit = 200000;
	int used_space = 80;
	int cb_map_min = 0;
	int cb_map_max = NUMBER_OF_ADDRESSABLE_BLOCKS() * BLOCK_SIZE;
	int cb_map_inc = NUMBER_OF_ADDRESSABLE_BLOCKS() * BLOCK_SIZE / 10;
	int max_copybacks = 3;

	stringstream space_usage_string;
	space_usage_string << used_space << "% used space";

	double start_time = Experiment::wall_clock_time();

	vector<Experiment_Result> exp;

	for (int copybacks = 0; copybacks <= max_copybacks; copybacks++) {
		MAX_REPEATED_COPY_BACKS_ALLOWED = copybacks;
		stringstream folder;
		stringstream expname;
		folder << exp_folder << "copybacks-" << copybacks << "/";
		if      (copybacks == 0) expname << "Copybacks disabled";
		else if (copybacks == 1) expname << copybacks << " copyback allowed";
		else                     expname << copybacks << " copybacks allowed";
		exp.push_back( Experiment::copyback_map_experiment(random_writes_reads_experiment, cb_map_min, cb_map_max, cb_map_inc, used_space, folder.str(), expname.str(), IO_limit) );
	}
	uint mean_pos_in_datafile = std::find(exp[0].column_names.begin(), exp[0].column_names.end(), "Write wait, mean (µs)") - exp[0].column_names.begin();
	assert(mean_pos_in_datafile != exp[0].column_names.size());

	uint gc_pos_in_datafile = std::find(exp[0].column_names.begin(), exp[0].column_names.end(), "Copybacks") - exp[0].column_names.begin();
	assert(gc_pos_in_datafile != exp[0].column_names.size());

	vector<int> used_space_values_to_show;
	for (int copybacks_in_map = cb_map_min; copybacks_in_map <= cb_map_max; copybacks_in_map += cb_map_inc)
		used_space_values_to_show.push_back(copybacks_in_map); // Show all used spaces values in multi-graphs

	int sx = 16;
	int sy = 8;

	chdir(exp_folder.c_str());
	Experiment::graph(sx, sy,   "Average throughput for random writes with different copyback parameters (" + space_usage_string.str() + ")", "throughput", 24, exp);
	Experiment::graph(sx, sy,   "Copybacks operations performed (" + space_usage_string.str() + ")", "copybacks", gc_pos_in_datafile, exp);
    Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram_allIOs", exp, 16000, true);
    Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram", exp, 16000, false);

	for (uint i = 0; i < exp.size(); i++) {
		chdir(exp[i].data_folder.c_str());
		Experiment::waittime_boxplot  		(sx, sy,   "Write latency boxplot", "boxplot", mean_pos_in_datafile, exp[i]);
		//Experiment_Runner::waittime_histogram		(sx, sy/2, "waittime-histograms", exp[i], used_space_values_to_show);
		Experiment::age_histogram			(sx, sy/2, "age-histograms", exp[i], used_space_values_to_show);
		Experiment::queue_length_history		(sx, sy/2, "queue_length", exp[i], used_space_values_to_show);
		Experiment::throughput_history		(sx, sy/2, "throughput_history", exp[i], used_space_values_to_show);
	}

	double end_time = Experiment::wall_clock_time();
	printf("=== Entire experiment finished in %s ===\n", Experiment::pretty_time(end_time - start_time).c_str());

	return 0;
}
