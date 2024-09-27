#include <complex>
#include <iostream>
#include <pthread.h>
#include <assert.h>
#include <sys/sysinfo.h>
// to get env variables
#include <cstdlib>

using namespace std;


//work package for each thread
struct thread_work{
	// start row
	int start;
	// end row
	int end;
	// calculated rows
	char** globalMat;
	//basic info
	int max_column;
	int max_row;
	int max_n;
	// array with all real components
	float* real_components;
	//debug
	int threadNo;
}; 
typedef struct thread_work thread_work_t;

void* work(void* tw){
	thread_work_t* w = (thread_work_t*) tw;

	for (int i=0; i < (w->end - w->start); i++){
		// global_row == row in end result matrix
		int global_row = i + w->start;

		// sanity check to have a happy compiler for malloc
		if((sizeof(char) * w->max_column > 0x7FFFFFFFFFFFFFFF)){
			fprintf(stderr, "Trying to allocate to much memory, exiting.\n");
			std::exit(1);
		};

		// use global row for r in original algo
		int r = global_row;

		// reserve space for the current row
		w->globalMat[r] = (char*) malloc(sizeof(char) * w->max_column);

		// sufficient to compute that once per row as it does not change any more
		float img_component = (float)r * 2 / w->max_row - 1;
		float real_component;
			for(int c = 0; c < w->max_column; ++c){
				complex<float> z= 0;
				
				real_component = w->real_components[c];
				// compute c to add 
				complex<float> co = complex<float>(
						real_component,
						img_component);
				int n = 0;
				while(abs(z) < 2.0 && ++n < (w->max_n)){
					z = z*z + co;
				}
				// write result to row
				w -> globalMat[r][c]=(n == w -> max_n ? '#' : '.');
			}
	}
	// avoid compiler warning
	return nullptr;

}


int main(){
	int max_row, max_column, max_n;
	cin >> max_row;
	cin >> max_column;
	cin >> max_n;

	bool debugMessages = false;
	if (char const *env_p = std::getenv("MANDELDEBUG")){
		std::string debug = env_p;
		if (debug == "true" || debug == "TRUE") debugMessages = true;
	}

	if(debugMessages) 
		fprintf(stderr, "=== Starting run of Mandelbrot ===\n");

	// Determine the amount of available CPUs
    int cpus = get_nprocs();
    // nprocs() might return wrong amount inside of a container.
    // Use MAX_CPUS instead, if available.
    if (getenv("MAX_CPUS")) {
        cpus = atoi(getenv("MAX_CPUS"));
    }
    // Sanity-check
    assert(cpus > 0 && cpus <= 64);
    if(debugMessages) 
		fprintf(stderr, "Running on %d CPUs\n", cpus);

	/*
		Ideal would be as many threads on real cpus as we have rows for full parallelism.
		But we likely have more rows than cpus. Thus, we should strive for as many threads
		so that we have a row/thread ratio close to one. This should be limited by a total 
		amount of threads in proportion to the amount of cpus we have. 
	*/
	const float MAX_THREAD_CPU_RATIO = 10.0;
	int amnt_threads = 0;
	if(cpus >= max_row){
		amnt_threads = max_row;
	}else{
		int row_thread_ratio = 0;
		float thread_cpu_ratio = 0.0;
		do{
			amnt_threads++;
			row_thread_ratio = max_row/amnt_threads;
			thread_cpu_ratio = (float)amnt_threads / cpus;
		}while(row_thread_ratio >= 1 && thread_cpu_ratio <= MAX_THREAD_CPU_RATIO);
		//subtract one thread as we broke the condition now
		amnt_threads--;
	}

	if(debugMessages)
		fprintf(stderr, "Running with %d threads\n", amnt_threads);


	// get space for complete result matrix
	char **mat = (char**) malloc(sizeof(char*) * max_row);
	
	//as many threads and thread work structs as we have cpus
	thread_work_t tw[amnt_threads];
	pthread_t threads[amnt_threads];

	//calculate real components ahead of time and only once
	float real_components[max_column];
	for (int c=0; c < max_column; c++){
		real_components[c] = (float)c * 2 / max_column - 1.5;
	}

	int rows_per_thread = max_row / amnt_threads;
	/*Above version of thread amount calculation creates cases where the remainder of 
	  max_row / amnt_threads is rather large. Avoid that by just add it on all
	  threads and not just hand it to the main thread
	*/
	int remainder = max_row % amnt_threads;
	int last_threads_end = 0;
	int end = 0;
	for(int i = 0; i < amnt_threads; i++){
		// set start and end row
		tw[i].start = last_threads_end;
		if(remainder > 0){
			end = tw[i].start + rows_per_thread + 1;
			remainder--;
		}else{
			end = tw[i].start + rows_per_thread;
		}
		tw[i].end = end;
		last_threads_end = end;
		 
		// get room for row arrays
		tw[i].globalMat = mat;

		// let thread know about real components
		tw[i].real_components = real_components;
		// enter basic info
		tw[i].max_column = max_column;
		tw[i].max_row = max_row;
		tw[i].max_n = max_n;
		tw[i].threadNo = i;

		if(debugMessages)
			fprintf(stderr, "Creating Thread %d from %d to %d\n", i, tw[i].start, tw[i].end-1);

		pthread_create(&threads[i], NULL, work, (void*)&tw[i]);
	}

	// collect work
	for (int i=0; i<amnt_threads; i++) {
		if(debugMessages)
			fprintf(stderr, "Collecting Thread %d from %d to %d\n", i, tw[i].start, tw[i].end-1);
		// wait for all threads
		pthread_join(threads[i], NULL);
	}

	// print result
	for(int r = 0; r < max_row; ++r){
		for(int c = 0; c < max_column; ++c)
			cout << mat[r][c];
		cout << '\n';
	}

	if(debugMessages)
		fprintf(stderr, "End of Run with %d CPUs\n", cpus);

}


