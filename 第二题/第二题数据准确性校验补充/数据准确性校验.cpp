#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <tbb/concurrent_queue.h>
#include <tbb/parallel_for.h>

// 计时器函数
auto time_cout() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

class DataProcessor {
private:
    tbb::concurrent_queue<std::vector<double>> data_queue;
    int num_threads;
    int num_iterations;
    std::vector<std::vector<double>> timing_data;
    std::mutex data_mutex;  // 添加一个 std::mutex 成员变量
    std::condition_variable data_cond;
    std::vector<std::vector<double>> random_numbers;  // 新增二维向量用于保存随机数
    std::vector<double> received_data;

public:
    DataProcessor(int threads, int iterations) : num_threads(threads), num_iterations(iterations) {
        timing_data.resize(num_threads, std::vector<double>(num_iterations * 2));
        random_numbers.resize(num_threads, std::vector<double>(1024)); // 初始化二维向量
    }

    void processData() {
        // 随机数生成器初始化
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(0.0, 1.0);

        // 创建10个线程的并行任务
        tbb::parallel_for(0, num_threads, 1, [&](int i) {
            for (int j = 0; j < num_iterations; ++j) {
                // 生成随机数并存储到二维向量中
                for (double& num : random_numbers[i]) {
                    num = dis(gen);
                }

                auto send_start = time_cout();
                // 将随机数发送给序号比自己大的下一个线程
                int next_thread_index = (i + 1) % num_threads;
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    data_queue.push(random_numbers[i]);
                }
                data_cond.notify_one();
                //auto send_end = time_cout();
                //auto send_duration = send_end + send_start ;
                //auto receive_start = time_cout()
                //std::vector<double> received_data;
                {
                    std::unique_lock<std::mutex> lock(data_mutex);
                    data_cond.wait(lock, [this] { return !data_queue.empty(); });
                    data_queue.try_pop(received_data);
                }

                auto receive_end = time_cout();
                auto receive_duration = (receive_end - send_start)*1.0/1000;

                // 存储发送和接收数据的时间到二维数组
                timing_data[i][j * 2 + 1] = receive_duration;
            }
            });
    }

    void writeCSV(std::string filename) {
        // 将数据写入CSV文件
        std::ofstream csv_file(filename);
        int count_less_than_5us = 0;
        int count_less_than_2us = 0;
        int count_less_than_1us = 0;
        double max_duration = 0.0;

        for (int i = 0; i < num_iterations; ++i) {
            for (int j = 0; j < num_threads; ++j) {
                double duration = timing_data[j][i * 2 + 1];

                // 在循环开始时写入当前循环次数
                if (j == 0) {
                    csv_file << i + 1;  // 写入循环次数，i + 1 是因为循环次数从1开始
                }

                // 写入计时数据
                csv_file << "," << duration;

                // 更新计数变量
                if (duration <= 5.0) {
                    count_less_than_5us++;
                }
                if (duration <= 2.0) {
                    count_less_than_2us++;
                }
                if (duration <= 1.0) {
                    count_less_than_1us++;
                }
                if (duration > max_duration) {
                    max_duration = duration;
                }

                // 在每行结束时添加换行符
                if (j == num_threads - 1) {
                    csv_file << std::endl;
                }
            }
        }

        double total_iterations = num_iterations * num_threads;
        double less_than_5us_percentage = (count_less_than_5us / total_iterations) * 100.0;
        double less_than_2us_percentage = (count_less_than_2us / total_iterations) * 100.0;
        double less_than_1us_percentage = (count_less_than_1us / total_iterations) * 100.0;

        csv_file << "Count less than 5us: " << count_less_than_5us << " (" << less_than_5us_percentage << "%)" << std::endl;
        csv_file << "Count less than 2us: " << count_less_than_2us << " (" << less_than_2us_percentage << "%)" << std::endl;
        csv_file << "Count less than 1us: " << count_less_than_1us << " (" << less_than_1us_percentage << "%)" << std::endl;
        csv_file << "Max duration: " << max_duration << std::endl;

        // 同时在控制台输出信息
        std::cout << "Count less than 5us: " << count_less_than_5us << " (" << less_than_5us_percentage << "%)" << std::endl;
        std::cout << "Count less than 2us: " << count_less_than_2us << " (" << less_than_2us_percentage << "%)" << std::endl;
        std::cout << "Count less than 1us: " << count_less_than_1us << " (" << less_than_1us_percentage << "%)" << std::endl;
        std::cout << "Max duration: " << max_duration << std::endl;

        csv_file.close();
    }

    void saveReceivedDataToCSV(const std::string& filename) {
        std::ofstream csv_file(filename);

        for (int i = 0; i < received_data.size(); ++i) {
            // 写入随机数值
                csv_file << received_data[i];
                // 在每列数据之间添加逗号
                    csv_file << ",";
            }

            // 在每行结束时添加换行符
            csv_file << std::endl;
        }
    

    void writeCSV_rand(std::string filename) {
        // 将数据写入CSV文件
        std::ofstream csv_file(filename);

        for (int i = 0; i < random_numbers.size(); ++i) {
            // 写入随机数值
            for (int j = 0; j < random_numbers[i].size(); ++j) {
                csv_file << random_numbers[i][j];

                // 在每列数据之间添加逗号
                if (j < random_numbers[i].size() - 1) {
                    csv_file << ",";
                }
            }

            // 在每行结束时添加换行符
            csv_file << std::endl;
        }
    }


};

int main() {
    // 创建DataProcessor对象
    DataProcessor processor(10, 50000);

    // 处理数据
    processor.processData();

    // 将数据写入CSV文件
    processor.writeCSV("timing_data.csv");
    processor.writeCSV_rand("send_random_numbers.csv");
    processor.saveReceivedDataToCSV("rev_random_numbers.csv");

    // 等待用户输入任意字符退出程序
    std::cout << "Enter any character to exit...";
    std::cin.get();  // 等待用户输入任意字符
    return 0;
}
