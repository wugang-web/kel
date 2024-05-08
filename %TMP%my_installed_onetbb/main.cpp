#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <tbb/concurrent_queue.h>
#include <tbb/parallel_for.h>

// ��ʱ������
auto time_cout() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

class DataProcessor {
private:
    tbb::concurrent_queue<std::vector<double>> data_queue;
    int num_threads;
    int num_iterations;
    std::vector<std::vector<double>> timing_data;
    std::mutex data_mutex;  // ���һ�� std::mutex ��Ա����
    std::condition_variable data_cond;

public:
    DataProcessor(int threads, int iterations) : num_threads(threads), num_iterations(iterations) {
        timing_data.resize(num_threads, std::vector<double>(num_iterations * 2));
    }

    void processData() {
        // �������������ʼ��
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(0.0, 1.0);

        // ����10���̵߳Ĳ�������
        
        tbb::parallel_for(0, num_threads, 1, [&](int i) {
            for (int j = 0; j < num_iterations; ++j) {
                // ���������
                std::vector<double> random_numbers(1024);
                for (double& num : random_numbers) {
                    num = dis(gen);
                }
                auto send_start = time_cout();
                // ����������͸���ű��Լ������һ���߳�
                int next_thread_index = (i + 1) % num_threads;
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    data_queue.push(random_numbers);
                }
                data_cond.notify_one();
                //auto send_end = time_cout();
                //auto send_duration = send_end + send_start ;
                //auto receive_start = time_cout()
                std::vector<double> received_data;
                {
                    std::unique_lock<std::mutex> lock(data_mutex);
                    data_cond.wait(lock, [this] { return !data_queue.empty(); });
                    data_queue.try_pop(received_data);
                }

                auto receive_end = time_cout();
                auto receive_duration = (receive_end - send_start)*1.0/1000;

                // �洢���ͺͽ������ݵ�ʱ�䵽��ά����
                //timing_data[i][j * 2] = send_duration;
                timing_data[i][j * 2 + 1] = receive_duration;
            }
            });
    }

    void writeCSV(std::string filename) {
        // ������д��CSV�ļ�
        std::ofstream csv_file(filename);
        for (int i = 0; i < num_iterations; ++i) {
            for (int j = 0; j < num_threads; ++j) {
                csv_file << timing_data[j][i * 2 + 1];
                if (j != num_threads - 1) {
                    csv_file << ",";
                }
                else {
                    csv_file << std::endl;
                }
            }
        }
        csv_file.close();
    }
};

int main() {
    // ����DataProcessor����
    DataProcessor processor(10, 50000);

    // ��������
    processor.processData();

    // ������д��CSV�ļ�
    processor.writeCSV("timing_data.csv");

    return 0;
}
