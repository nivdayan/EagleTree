
#include "ssd.h"
#include <unistd.h>   // chdir
#include <sys/stat.h> // mkdir

using namespace ssd;

/*vector<Thread*>  random_writes_experiment(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 0;
	long num_IOs = numeric_limits<int>::max();
	Thread* t1 = new Asynchronous_Sequential_Thread(0, highest_lba, 1, WRITE, IO_submission_rate, 1);
	t1->add_follow_up_thread(new Asynchronous_Random_Thread(0, highest_lba, num_IOs, 2, WRITE, IO_submission_rate, 1));
	vector<Thread*> threads;
	threads.push_back(t1);
	return threads;
}*/

vector<Thread*>  random_writes_reads_experiment(int highest_lba, double IO_submission_rate) {
	BLOCK_MANAGER_ID = 0;
	long num_IOs = numeric_limits<int>::max();
	Thread* t1 = new Asynchronous_Sequential_Thread(0, highest_lba, 1, WRITE, IO_submission_rate, 1);

	Thread* random_formatter = new Asynchronous_Random_Thread_Reader_Writer(0, highest_lba, highest_lba * 3, 4246);
	t1->add_follow_up_thread(random_formatter);

	Thread* experiment_thread = new Asynchronous_Random_Thread_Reader_Writer(0, highest_lba, num_IOs, 2364);
	experiment_thread->set_experiment_thread(true);
	random_formatter->add_follow_up_thread(experiment_thread);

	vector<Thread*> threads;
	threads.push_back(t1);
	return threads;
}

vector<Thread*>  naive_lazy(int highest_lba, double IO_submission_rate) {
	GREED_SCALE = 0;
	SCHEDULING_SCHEME = 0;
	return random_writes_reads_experiment(highest_lba, IO_submission_rate);
}

vector<Thread*>  naive_greedy(int highest_lba, double IO_submission_rate) {
	GREED_SCALE = 1;
	SCHEDULING_SCHEME = 0;
	return random_writes_reads_experiment(highest_lba, IO_submission_rate);
}

vector<Thread*>  smart_lazy(int highest_lba, double IO_submission_rate) {
	GREED_SCALE = 0;
	SCHEDULING_SCHEME = 2;
	return random_writes_reads_experiment(highest_lba, IO_submission_rate);
}

vector<Thread*>  smart_greedy(int highest_lba, double IO_submission_rate) {
	GREED_SCALE = 1;
	SCHEDULING_SCHEME = 2;
	return random_writes_reads_experiment(highest_lba, IO_submission_rate);
}



int main()
{
	string exp_folder  = "exp_scheduling/";
	Experiment::graph_filename_prefix = "exp_scheduling-";
	mkdir(exp_folder.c_str(), 0755);

	load_config();

	SSD_SIZE = 4;
	PACKAGE_SIZE = 2;
	DIE_SIZE = 1;
	PLANE_SIZE = 128;
	BLOCK_SIZE = 32;

	PAGE_READ_DELAY = 50;
	PAGE_WRITE_DELAY = 200;
	BUS_CTRL_DELAY = 5;
	BUS_DATA_DELAY = 90;
	BLOCK_ERASE_DELAY = 1500;

	int IO_limit = 2000;
	int space_min = 80;
	int space_max = 85;
	int space_inc = 5;

	PRINT_LEVEL = 0;
	MAX_SSD_QUEUE_SIZE = 15;
	MAX_REPEATED_COPY_BACKS_ALLOWED = 0;

	double start_time = Experiment::wall_clock_time();

	PRINT_LEVEL = 0;

	vector<Experiment_Result> exp;
	exp.push_back( Experiment::overprovisioning_experiment(naive_lazy,	space_min, space_max, space_inc, exp_folder + 		"naive_lazy/", "naive lazy", IO_limit) );
	exp.push_back( Experiment::overprovisioning_experiment(naive_greedy,	space_min, space_max, space_inc, exp_folder + 		"naive_greedy/", "naive greedy", IO_limit) );
	exp.push_back( Experiment::overprovisioning_experiment(smart_lazy,	space_min, space_max, space_inc, exp_folder + 		"smart_lazy/", "smart lazy", IO_limit) );
	exp.push_back( Experiment::overprovisioning_experiment(smart_greedy,	space_min, space_max, space_inc, exp_folder + 		"smart_greedy/", "smart greedy", IO_limit) );

	// Print column names for info
	for (uint i = 0; i < exp[0].column_names.size(); i++)
		printf("%d: %s\n", i, exp[0].column_names[i].c_str());

	uint mean_pos_in_datafile = std::find(exp[0].column_names.begin(), exp[0].column_names.end(), "Write latency, mean (µs)") - exp[0].column_names.begin();
	assert(mean_pos_in_datafile != exp[0].column_names.size());

	vector<int> used_space_values_to_show;
	for (int i = space_min; i <= space_max; i += space_inc)
		used_space_values_to_show.push_back(i); // Show all used spaces values in multi-graphs

	int sx = 16;
	int sy = 8;

	chdir(exp_folder.c_str());

	Experiment::graph(sx, sy,   "Throughput", 				"throughput", 			24, exp, 30);
	Experiment::graph(sx, sy,   "Write Throughput", 			"throughput_write", 	25, exp, 30);
	Experiment::graph(sx, sy,   "Read Throughput", 			"throughput_read", 		26, exp, 30);
	Experiment::graph(sx, sy,   "Num Erases", 				"num_erases", 			8, 	exp, 16000);
	Experiment::graph(sx, sy,   "Num Migrations", 			"num_migrations", 		3, 	exp, 500000);

	Experiment::graph(sx, sy,   "Write latency, mean", 			"Write latency, mean", 		9, 	exp, 12000);
	Experiment::graph(sx, sy,   "Write latency, max", 			"Write latency, max", 		14, exp, 40000);
	Experiment::graph(sx, sy,   "Write latency, std", 			"Write latency, std", 		15, exp, 14000);

	Experiment::graph(sx, sy,   "Read latency, mean", 			"Read latency, mean", 		16,	exp, 2000);
	Experiment::graph(sx, sy,   "Read latency, max", 			"Read latency, max",		21,	exp, 70000);
	Experiment::graph(sx, sy,   "Read latency, stdev", 			"Read latency, stdev", 		22,	exp, 4000);

	// VALUES FOR THE TWO LAST PARAMETERS IN cross_experiment_waittime_histogram() and waittime_histogram():
	// 1. Application IOs, Reads+writes
	// 2. Application IOs, Writes
	// 3. Application IOs, Reads
	// 4. Internal operations, All
	// 5. Internal operations, Writes
	// 6. Internal operations, Reads

	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 90", exp, 90, 1, 4);
	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 80", exp, 80, 1, 4);
	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 70", exp, 70, 1, 4);
	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 60", exp, 60, 1, 4);


	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram RW 90", exp, 90, 2, 3);
	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram RW 80", exp, 80, 2, 3);
	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram RW 70", exp, 70, 2, 3);
	Experiment::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram RW 60", exp, 60, 2, 3);

	for (uint i = 0; i < exp.size(); i++) {
		chdir(exp[i].data_folder.c_str());
		Experiment::waittime_boxplot  		(sx, sy,   "Write latency boxplot", "boxplot", mean_pos_in_datafile, exp[i]);
		Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms-allIOs", exp[i], used_space_values_to_show, 1, 4);
		Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms-appIOs", exp[i], used_space_values_to_show, 1);
		Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms-rw-appIOs", exp[i], used_space_values_to_show, 2, 3);
		Experiment::age_histogram			(sx, sy/2, "age_histograms", exp[i], used_space_values_to_show);
		Experiment::queue_length_history		(sx, sy/2, "queue_length", exp[i], used_space_values_to_show);
		Experiment::throughput_history		(sx, sy/2, "throughput_history", exp[i], used_space_values_to_show);

		Experiment::latency_plot(sx, sy, "IO Latency", "latency-write-80",      1, 80, exp[i]);
		Experiment::latency_plot(sx, sy, "IO Latency", "latency-read-80",       2, 80, exp[i]);
		Experiment::latency_plot(sx, sy, "IO Latency", "latency-read-write-80", 3, 80, exp[i]);
		Experiment::latency_plot(sx, sy, "IO Latency", "latency-write-85",      1, 85, exp[i]);
		Experiment::latency_plot(sx, sy, "IO Latency", "latency-read-85",       2, 85, exp[i]);
		Experiment::latency_plot(sx, sy, "IO Latency", "latency-read-write-85", 3, 85, exp[i]);
	}

	double end_time = Experiment::wall_clock_time();
	printf("=== Entire experiment finished in %s ===\n", Experiment::pretty_time(end_time - start_time).c_str());

	return 0;
}
