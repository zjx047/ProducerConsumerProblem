#include <iostream>
#include <semaphore>
#include <cstdlib>
#include <thread>
#include <vector>

using namespace std;

// Global variables
int buffer_size;
int timeout_seconds = 10;

int* buffer;  // Shared buffer for jobs
int head = 0; // Index for the next item to be consumed
int tail = 0; // Index for the next free space to add an item
int count = 0; // Number of items in the buffer


/**
 * @brief Producer function that generates jobs and adds them to the buffer.
 * 
 * This function will generate a specified number of jobs and attempt to add them to the shared buffer. 
 * The producer will wait for space to become available in the buffer, with a timeout. If space is not 
 * available within the timeout, the producer will quit. Jobs are added one by one, and the producer will 
 * signal the consumer once a job is added.
 *
 * @param jobs_to_generate Number of jobs the producer will generate.
 * @param producer_id Unique identifier for the producer thread.
 * @param space Semaphore to track the available space in the buffer.
 * @param item Semaphore to track the available items in the buffer.
 * @param buffer_mutex Binary semaphore to protect access to the buffer.
 */
void producer(int jobs_to_generate, int producer_id, counting_semaphore<>& space, counting_semaphore<>& item, binary_semaphore& buffer_mutex) {
    for (int i = 0; i < jobs_to_generate; ++i) {
        int job = rand() % 10 + 1;

        while (true) {

            // Wait for space in buffer to become available, with timeout
            if (space.try_acquire_for(chrono::seconds(timeout_seconds))) {

                // Critical section: Lock buffer access to ensure exclusive write
                buffer_mutex.acquire();

                // Add the job to the buffer and update relevant pointers
                buffer[tail] = job;
                tail = (tail + 1) % buffer_size;
                count++;
                cout << "Producer " << producer_id << " added job: '" << job
                    << "' to buffer. Buffer now has " << count << (count == 1 ? " job." : " jobs.") << endl;

                // Unlock buffer access and signal that an item is available
                buffer_mutex.release();
                item.release();
                break;
            } else {
                cout << "Producer " << producer_id << " quitting due to timeout!" << endl;
                return;
            }
        }
    }
    cout << "Producer " << producer_id << " has successfully produced all " << jobs_to_generate 
            << (jobs_to_generate == 1 ? " job." : " jobs.") << endl;
}


/**
 * @brief Consumer function that consumes jobs from the buffer and processes them.
 * 
 * This function continuously consumes jobs from the buffer. It waits for an item to become available 
 * in the buffer, with a timeout. If no item is available within the timeout, the consumer stops. Once an 
 * item is consumed, the consumer will simulate processing the job by sleeping for a duration based on the job's value.
 *
 * @param consumer_id Unique identifier for the consumer thread.
 * @param space Semaphore to track the available space in the buffer.
 * @param item Semaphore to track the available items in the buffer.
 * @param buffer_mutex Binary semaphore to protect access to the buffer.
 */
void consumer(int consumer_id, counting_semaphore<>& space, counting_semaphore<>& item, binary_semaphore& buffer_mutex) {
    while (true) {
        // Wait for an item to become available, with timeout
        if (item.try_acquire_for(chrono::seconds(timeout_seconds))) {
            buffer_mutex.acquire();
            if (count > 0) { // Do I need this?
                int job = buffer[head];
                head = (head + 1) % buffer_size;
                count--;
                cout << "Consumer " << consumer_id << " consumed job: '" << job
                        << "' from buffer. Buffer now has " << count << " jobs left." << endl;

                // Simulate processing job
                this_thread::sleep_for(chrono::seconds(job));
            }
            buffer_mutex.release();
            space.release();
        } else {
            cout << "Consumer " << consumer_id << " quitting due to timeout!" << endl;
            return;
        }
    }
}


/**
 * @brief Main function that initializes resources and launches producer and consumer threads.
 * 
 * This function handles command-line arguments, initializes the buffer and semaphores, and launches 
 * the specified number of producer and consumer threads. It ensures that all threads complete their 
 * work before the program exits. Once the threads have finished, the buffer memory is cleaned up.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return int Exit status of the program (0 on success, 1 on failure).
 */
int main(int argc, char* argv[]) {

    // Read in command line arguments
    int jobs_per_producer, num_producers, num_consumers;
    if (argc != 5) {
        cerr << "Usage: " << argv[0] << " <buffer_size> <jobs_per_producer> <num_producers> <num_consumers>" << endl;
        return 1;
    }

    try {
        buffer_size = stoi(argv[1]);
        jobs_per_producer = stoi(argv[2]);
        num_producers = stoi(argv[3]);
        num_consumers = stoi(argv[4]);
    } catch (const invalid_argument& e) {
        cerr << "Invalid argument: Please provide valid integers for all parameters!" << endl;
        return 1;
    } catch (const exception& e) {
        cerr << "An error occurred: " << e.what() << endl;
        return 1;
    }

    // Check if any of the arguments are negative or zero
    if (buffer_size <= 0 || jobs_per_producer <= 0 || num_producers <= 0 || num_consumers <= 0) {
        cerr << "All values must be positive integers!" << endl;
        return 1;
    }

    // Echo the validated input arguments back to the user
    cout << "Buffer size: " << buffer_size << endl;
    cout << "Jobs per producer: " << jobs_per_producer << endl;
    cout << "Number of producers: " << num_producers << endl;
    cout << "Number of consumers: " << num_consumers << endl;

    // Initialise buffer dynamically
    try {
        buffer = new int[buffer_size];
    } catch (const bad_alloc& e) {
        cerr << "Memory allocation failed: " << e.what() << endl;
        return 1;
    }

    // Initialise semaphores
    counting_semaphore<> space(buffer_size);  // Semaphore to track available space in the buffer
    counting_semaphore<> item(0);  // Semaphore to track available items in the buffer
    binary_semaphore buffer_mutex(1); // Binary semaphore to lock/unlock the buffer (for mutual exclusion)

    // Launch producer threads to generate jobs
    vector<thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        try {
            producers.push_back(thread(producer, jobs_per_producer, i + 1, ref(space), ref(item), ref(buffer_mutex)));
        } catch (const system_error& e) {
            cerr << "Thread creation failed: " << e.what() << endl;
            return 1;
        }
    }

    // Launch consumer threads to consume jobs from buffer
    vector<thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        try {
            consumers.push_back(thread(consumer, i + 1, ref(space), ref(item), ref(buffer_mutex)));
        } catch (const system_error& e) {
            cerr << "Thread creation failed: " << e.what() << endl;
            return 1;
        }
    }

    // Ensure the main thread waits for all producer and consumer threads to finish
    for (thread& t : producers) {
        t.join();
    }
    for (thread& t : consumers) {
        t.join();
    }

    delete[] buffer;
    return 0;
}
