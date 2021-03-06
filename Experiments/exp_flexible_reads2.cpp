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


#include "ssd.h"
#include <unistd.h>   // chdir
#include <sys/stat.h> // mkdir
#include <sstream>

using namespace ssd;

// Internal division of space in each grace hash join instance
double r1 = .20; // Relation 1 percentage use of addresses
double r2 = .20; // Relation 2 percentage use of addresses
double fs = .60; // Free space percentage use of addresses

// Overall division of space
double gh = .5; // Grace hash join(s) percentage use of addresses
double ns = .5; // "Noise space" percentage use of addresses
//------------;
// total  =1.0

int num_grace_hash_join_threads;

StatisticsGatherer* read_statistics_gatherer;

vector<Thread*> normal_exp(int highest_lba) {
	Grace_Hash_Join::initialize_counter();

	Thread* relation1_write    	= new Asynchronous_Sequential_Writer(0, highest_lba);

	Thread* experiment_thread    = new Synchronous_Sequential_Writer(0, highest_lba);
	experiment_thread->set_experiment_thread(true);

	relation1_write->add_follow_up_thread(experiment_thread);

	vector<Thread*> threads;
	threads.push_back(relation1_write);

	return threads;
}

vector<Thread*> flexible_reads_exp(int highest_lba) {
	Grace_Hash_Join::initialize_counter();

	Thread* relation1_write  	= new Asynchronous_Sequential_Writer(0, highest_lba);

	Thread* flexible_reads = new Flexible_Reader_Thread(0, highest_lba, INFINITE);

	flexible_reads->set_experiment_thread(true);

	relation1_write->add_follow_up_thread(flexible_reads);

	vector<Thread*> threads;
	threads.push_back(relation1_write);

	return threads;
}


int main()
{
	load_config();

	SSD_SIZE = 4;
	PACKAGE_SIZE = 2;
	DIE_SIZE = 1;
	PLANE_SIZE = 64 * 2;
	BLOCK_SIZE = 32 * 2;

	PAGE_READ_DELAY = 50;
	PAGE_WRITE_DELAY = 200;
	BUS_CTRL_DELAY = 5;
	BUS_DATA_DELAY = 100;
	BLOCK_ERASE_DELAY = 1500;

	int IO_limit = 10000;

	int write_threads_min = 0;
	int write_threads_max = 4;
	double used_space = .80; // overprovisioning level for variable random write threads experiment

	PRINT_LEVEL = 0;
	MAX_SSD_QUEUE_SIZE = 32;
	MAX_REPEATED_COPY_BACKS_ALLOWED = 0;
	SCHEDULING_SCHEME = 2;
	GREED_SCALE = 2;
	USE_ERASE_QUEUE = false;
	BLOCK_MANAGER_ID = 0;

	const int num_pages = NUMBER_OF_ADDRESSABLE_BLOCKS() * BLOCK_SIZE;
	const int avail_pages = num_pages * used_space;

	/*num_grace_hash_join_threads = 1;
	Experiment_Runner::random_writes_on_the_side_experiment(grace_hash_join,  0, 0, 1, "__/", "None", IO_limit, used_space, avail_pages*ns+1, avail_pages);
	return 1;*/

	for (num_grace_hash_join_threads = 1; num_grace_hash_join_threads <= 1; num_grace_hash_join_threads++) {
		stringstream num_grace_hash_join_text;
		num_grace_hash_join_text << num_grace_hash_join_threads;

		string exp_folder  = "flexible_reads_threads/";
		mkdir(exp_folder.c_str(), 0755);

		double start_time = Experiment::wall_clock_time();

		vector<vector<Experiment_Result> > exps;

		exps.push_back( Experiment::random_writes_on_the_side_experiment(flexible_reads_exp,		write_threads_min, write_threads_max, 1, exp_folder + "None/", "None",                  			IO_limit, used_space, avail_pages*ns+1, avail_pages) );
		exps.push_back( Experiment::random_writes_on_the_side_experiment(normal_exp,	  			write_threads_min, write_threads_max, 1, exp_folder + "Flexible_reads/", "Flexible reads",        IO_limit, used_space, avail_pages*ns+1, avail_pages) );


		uint mean_pos_in_datafile = std::find(exps[0][0].column_names.begin(), exps[0][0].column_names.end(), "Write latency, mean (µs)") - exps[0][0].column_names.begin();
		assert(mean_pos_in_datafile != exps[0][0].column_names.size());

		vector<int> num_write_thread_values_to_show;
		for (int i = write_threads_min; i <= write_threads_max; i += 1)
			num_write_thread_values_to_show.push_back(i); // Show all used spaces values in multi-graphs

		int sx = 16;
		int sy = 8;

		for (int i = 0; i < exps[0][0].column_names.size(); i++) printf("%d: %s\n", i, exps[0][0].column_names[i].c_str());

		chdir(exp_folder.c_str());
		for (int i = 0; i < exps[0].size(); ++i) { // i = 0: GLOBAL, i = 1: EXPERIMENT, i = 2: WRITE_THREADS
			vector<Experiment_Result> exp;
			for (int j = 0; j < exps.size(); ++j) exp.push_back(exps[j][i]);
			if      (i == 1) { mkdir("Experiment_Threads",    0755); chdir("Experiment_Threads"); }
			else if (i == 2) { mkdir("Noise_Threads", 0755); chdir("Noise_Threads"); }
			Experiment::graph(sx, sy,   "Throughput", 				"throughput", 			24, exp/*, 30*/, UNDEFINED);
			Experiment::graph(sx, sy,   "Write Throughput", 			"throughput_write", 	25, exp/*, 30*/);
			Experiment::graph(sx, sy,   "Read Throughput", 			"throughput_read", 		26, exp/*, 30*/);
			Experiment::graph(sx, sy,   "Num Erases", 				"num_erases", 			8, 	exp/*, 16000*/);
			Experiment::graph(sx, sy,   "Num Migrations", 			"num_migrations", 		3, 	exp/*, 500000*/);

			Experiment::graph(sx, sy,   "Write latency, mean", 			"Write latency, mean", 		9, 	exp);
			Experiment::graph(sx, sy,   "Write latency, max", 			"Write latency, max", 		14, exp);
			Experiment::graph(sx, sy,   "Write latency, std", 			"Write latency, std", 		15, exp);

			Experiment::graph(sx, sy,   "Read latency, mean", 			"Read latency, mean", 		16,	exp);
			Experiment::graph(sx, sy,   "Read latency, max", 			"Read latency, max", 		21, exp);
			Experiment::graph(sx, sy,   "Read latency, std", 			"Read latency, std", 		22, exp);

			Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 90", exp, 90, 1, 4);
			Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 80", exp, 80, 1, 4);
			Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 70", exp, 70, 1, 4);
			Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 70", exp, 60, 1, 4);
			if (i > 0) { chdir(".."); }
		}
		vector<Experiment_Result>& exp = exps[0]; // Global one
		for (uint i = 0; i < exp.size(); i++) {
			printf("%s\n", exp[i].data_folder.c_str());
			chdir(exp[i].data_folder.c_str());
			Experiment::waittime_boxplot  		(sx, sy,   "Write latency boxplot", "boxplot", mean_pos_in_datafile, exp[i]);
			Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms-allIOs", exp[i], num_write_thread_values_to_show, 1, 4);
			Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms-allIOs", exp[i], num_write_thread_values_to_show, true);
			Experiment::age_histogram			(sx, sy/2, "age-histograms", exp[i], num_write_thread_values_to_show);
			Experiment::queue_length_history		(sx, sy/2, "queue_length", exp[i], num_write_thread_values_to_show);
			Experiment::throughput_history		(sx, sy/2, "throughput_history", exp[i], num_write_thread_values_to_show);
			chdir("../..");
		}
		double end_time = Experiment::wall_clock_time();
		printf("=== Entire experiment finished in %s ===\n", Experiment::pretty_time(end_time - start_time).c_str());

		chdir(".."); // Leaving
	}
	return 0;
}

