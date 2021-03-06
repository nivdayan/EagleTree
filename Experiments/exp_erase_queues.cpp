/* Copyright 2009, 2010 Brendan Tauras */

/* run_test2.cpp is part of FlashSim. */

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
 * Brendan Tauras 2010-08-03
 *
 * driver to create and run a very basic test of writes then reads */

#include "ssd.h"
#include <unistd.h>   // chdir
#include <sys/stat.h> // mkdir

using namespace ssd;


// problem: some of the pointers for the 6 block managers end up in the same LUNs. This is stupid.
// solution: have a method in bm_parent that returns a free block from the LUN with the shortest queue.

vector<Thread*> sequential_experiment(int highest_lba, double IO_submission_rate) {
	long log_space_per_thread = highest_lba / 2;
	long max_file_size = log_space_per_thread / 4;
	long num_files = 200000;

	//Thread* fm1 = new File_Manager(0, log_space_per_thread, num_files, max_file_size, IO_submission_rate, 1, 1);
	//Thread* fm2 = new File_Manager(log_space_per_thread + 1, log_space_per_thread * 2, num_files, max_file_size, IO_submission_rate, 2, 2);

	Thread* t1 = new Asynchronous_Sequential_Thread(0, log_space_per_thread * 2, 1, WRITE, 100, 0);

	t1->add_follow_up_thread(new File_Manager(0, log_space_per_thread, num_files, max_file_size, IO_submission_rate, 1, 1));

	//t1->add_follow_up_thread(new File_Manager(log_space_per_thread + 1, log_space_per_thread * 2, num_files, max_file_size, IO_submission_rate, 1, 2));

	t1->add_follow_up_thread(new Asynchronous_Random_Thread(log_space_per_thread + 1, log_space_per_thread * 2, 50000000, 2, WRITE, IO_submission_rate * 2, 1));


	vector<Thread*> threads;
	//threads.push_back(fm1);
	//threads.push_back(fm2);
	threads.push_back(t1);
	return threads;
}

vector<Thread*> tagging(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 3;
	GREEDY_GC = true;
	ENABLE_TAGGING = true;
	WEARWOLF_LOCALITY_THRESHOLD = 10;
	LOCALITY_PARALLEL_DEGREE = 0;
	return sequential_experiment(highest_lba, IO_submission_rate);
}

vector<Thread*> shortest_queues(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 0;
	GREEDY_GC = true;
	return sequential_experiment(highest_lba, IO_submission_rate);
}

vector<Thread*> detection_LUN(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 3;
	GREEDY_GC = true;
	ENABLE_TAGGING = false;
	WEARWOLF_LOCALITY_THRESHOLD = 10;
	LOCALITY_PARALLEL_DEGREE = 2;
	return sequential_experiment(highest_lba, IO_submission_rate);
}

vector<Thread*> detection_CHANNEL(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 3;
	GREEDY_GC = true;
	ENABLE_TAGGING = false;
	WEARWOLF_LOCALITY_THRESHOLD = 10;
	LOCALITY_PARALLEL_DEGREE = 1;
	return sequential_experiment(highest_lba, IO_submission_rate);
}

vector<Thread*> detection_BLOCK(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 3;
	GREEDY_GC = true;
	ENABLE_TAGGING = false;
	WEARWOLF_LOCALITY_THRESHOLD = 10;
	LOCALITY_PARALLEL_DEGREE = 0;
	return sequential_experiment(highest_lba, IO_submission_rate);
}

int main()
{
	string exp_folder  = "exp_erase_queues/";
	mkdir(exp_folder.c_str(), 0755);

	load_config();

	/*sequential_tagging
	 * sequential_shortest_queues
		sequential_detection_LUN
		sequential_detection_CHANNEL
		sequential_detection_BLOCK*/


	SSD_SIZE = 4;
	PACKAGE_SIZE = 2;
	DIE_SIZE = 1;
	PLANE_SIZE = 256;
	BLOCK_SIZE = 32;

	PAGE_READ_DELAY = 5;
	PAGE_WRITE_DELAY = 20;
	BUS_CTRL_DELAY = 1;
	BUS_DATA_DELAY = 8;
	BLOCK_ERASE_DELAY = 150;

	int IO_limit = 100000 * 4;
	int space_min = 70;
	int space_max = 90;
	int space_inc = 2;

	double start_time = Experiment::wall_clock_time();

	PRINT_LEVEL = 0;
	MAX_SSD_QUEUE_SIZE = 15;

	vector<Experiment_Result> exp;

	PRIORITISE_GC = false;
	USE_ERASE_QUEUE = true;
	exp.push_back( Experiment::overprovisioning_experiment(tagging, 			space_min, space_max, space_inc, exp_folder + "oracle_erase/",			"Oracle Erase Queue", IO_limit) );

	USE_ERASE_QUEUE = false;
	exp.push_back( Experiment::overprovisioning_experiment(tagging, 			space_min, space_max, space_inc, exp_folder + "oracle/",			"Oracle", IO_limit) );




	//exp.push_back( Experiment_Runner::overprovisioning_experiment(shortest_queues,	space_min, space_max, space_inc, exp_folder + "shortest_queues/",	"Shortest Queues", IO_limit) );
	//space_max = 90;
	//exp.push_back( Experiment_Runner::overprovisioning_experiment(detection_LUN, 	space_min, space_max, space_inc, exp_folder + "seq_detect_lun/",	"Seq Detect: LUN", IO_limit) );
	//exp.push_back( Experiment_Runner::overprovisioning_experiment(detection_CHANNEL, space_min, space_max, space_inc, exp_folder + "seq_detect_channel/","Seq Detect: CHANNEL", IO_limit) );
	//exp.push_back( Experiment_Runner::overprovisioning_experiment(detection_BLOCK, 	space_min, space_max, space_inc, exp_folder + "seq_detect_block/",	"Seq Detect: SSD", IO_limit) );

	uint mean_pos_in_datafile = std::find(exp[0].column_names.begin(), exp[0].column_names.end(), "Write wait, mean (µs)") - exp[0].column_names.begin();
	assert(mean_pos_in_datafile != exp[0].column_names.size());

	vector<int> used_space_values_to_show;
	for (int i = space_min; i <= space_max; i += space_inc)
		used_space_values_to_show.push_back(i); // Show all used spaces values in multi-graphs

	int sx = 16;
	int sy = 8;

	//for (int i = 0; i < exp[0].column_names.size(); i++) printf("%d: %s\n", i, exp[0].column_names[i].c_str());

	chdir(exp_folder.c_str());
	Experiment::graph(sx, sy,   "Block Manager Effect on Average Throughput, for mixed SW and RW", "throughput", 24, exp);

	Experiment::graph(sx, sy,   "Total number of erases", "num_erases", 8, exp);

	Experiment::graph(sx, sy,   "Latency standard dev", "latency", 15, exp);

	Experiment::graph(sx, sy,   "Write wait, Q25", "Write wait, Q25 (µs)", 11, exp);
	Experiment::graph(sx, sy,   "Write wait, Q50", "Write wait, Q50 (µs)", 12, exp);
	Experiment::graph(sx, sy,   "Write wait, Q75", "Write wait, Q75 (µs)", 13, exp);
	Experiment::graph(sx, sy,   "Write wait, max", "Write wait, max (µs)", 14, exp);

	for (uint i = 0; i < exp.size(); i++) {
		printf("%s\n", exp[i].data_folder.c_str());
		chdir(exp[i].data_folder.c_str());
		Experiment::waittime_boxplot  		(sx, sy,   "Write latency boxplot", "boxplot", mean_pos_in_datafile, exp[i]);
		Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms", exp[i], used_space_values_to_show);
		Experiment::age_histogram			(sx, sy/2, "age-histograms", exp[i], used_space_values_to_show);
		Experiment::queue_length_history		(sx, sy/2, "queue_length", exp[i], used_space_values_to_show);
		Experiment::throughput_history		(sx, sy/2, "throughput_history", exp[i], used_space_values_to_show);
	}

	double end_time = Experiment::wall_clock_time();
	printf("=== Entire experiment finished in %s ===\n", Experiment::pretty_time(end_time - start_time).c_str());

	return 0;
}
