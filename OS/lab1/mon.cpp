#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

using namespace std;

class Task {
public:
    int task_id;
    Task(int id) {
        task_id = id;
    }
};

class Buffer {
private:
    mutex my_mutex;           
    condition_variable queue; 
    
    Task* current_task = nullptr; 

public:
    void put(Task* t) {
        unique_lock<mutex> ul(my_mutex);

        while (current_task != nullptr) {
            queue.wait(ul);
        }

        current_task = t;
        cout << "Generatr -> created task #" << t->task_id << endl;

        queue.notify_one();
    }

    Task* get() {
        unique_lock<mutex> ul(my_mutex);

        while (current_task == nullptr) {
            queue.wait(ul);
        }

        Task* t = current_task;
        current_task = nullptr; 

        cout << "Worker   <- processed task #" << t->task_id << endl;

        queue.notify_one();

        return t;
    }
};

void runGenerator(Buffer& buf) {
    int i = 1;
    while (true) {
        this_thread::sleep_for(chrono::seconds(1));
        
        Task* t = new Task(i++);
        buf.put(t);
    }
}

void runWorker(Buffer& buf) {
    while (true) {
        Task* t = buf.get();
        delete t;
    }
}

int main() {
    Buffer sharedBuffer;

    cout << "System started..." << endl;

    thread t1(runGenerator, ref(sharedBuffer));
    thread t2(runWorker, ref(sharedBuffer));

    t1.join();
    t2.join();

    return 0;
}
