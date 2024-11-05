#include <iostream> // 引用輸入輸出流函式庫
#include <chrono> // 引用時間函式庫
#include <thread> // 引用執行緒函式庫
#include <atomic> // 引用原子操作函式庫
#include <mach/mach.h> // 引用 Mach 核心 API
#include <mach/host_info.h> // 引用主機資訊 API
#include <sys/sysctl.h> // 引用系統控制 API
#include <sys/statvfs.h>  // 引用檔案系統統計 API (用於磁碟使用量)
#include <ifaddrs.h>      // 引用網路介面地址 API (用於網路使用量)
#include <net/if.h> // 引用網路介面 API
#include <net/if_dl.h> // 引用資料鏈路介面 API
#include <cstdlib> // 引用標準函式庫 (用於 popen, pclose)
#include <sstream> // 引用字串流函式庫
#include <string> // 引用字串函式庫


std::atomic<bool> keepRunning(true); // 控制程式是否繼續執行的原子變數，初始化為 true

// 取得 CPU 使用率
double getCpuUsage() {
    host_cpu_load_info_data_t cpuInfo; // 宣告一個結構體變數來存放 CPU 負載資訊
    mach_msg_type_number_t infoCount = HOST_CPU_LOAD_INFO_COUNT; // 宣告一個變數來存放 CPU 負載資訊的數量

    // 呼叫 host_statistics() 取得 CPU 負載資訊
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuInfo, &infoCount) != KERN_SUCCESS) {
        std::cerr << "Failed to get CPU load information" << std::endl; // 輸出錯誤訊息
        return -1.0; // 返回 -1.0 表示取得 CPU 使用率失敗
    }

    uint64_t totalTicks = 0; // 宣告一個變數來存放 CPU 總共的 ticks 數
    for (int i = 0; i < CPU_STATE_MAX; i++) { // 迴圈計算 CPU 總共的 ticks 數
        totalTicks += cpuInfo.cpu_ticks[i]; // 將每個狀態的 ticks 數加總
    }

    // 計算 CPU 使用率
    double cpuUsage = 100.0 * (cpuInfo.cpu_ticks[CPU_STATE_USER] + cpuInfo.cpu_ticks[CPU_STATE_SYSTEM]) / totalTicks;
    return cpuUsage; // 返回 CPU 使用率
}

// 取得記憶體使用量
double getMemoryUsage() {
    mach_task_basic_info_data_t info; // 宣告一個結構體變數來存放任務基本資訊
    mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT; // 宣告一個變數來存放任務基本資訊的數量

    // 呼叫 task_info() 取得任務基本資訊
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &size) != KERN_SUCCESS) {
        std::cerr << "Failed to get memory information" << std::endl; // 輸出錯誤訊息
        return -1.0; // 返回 -1.0 表示取得記憶體使用量失敗
    }

    // 計算記憶體使用量 (單位: MB)
    double memoryUsage = info.resident_size / (1024.0 * 1024.0);
    return memoryUsage; // 返回記憶體使用量
}

// 取得磁碟使用情況
double getDiskUsage() {
    struct statvfs stat; // 宣告一個結構體變數來存放檔案系統統計資訊
    if (statvfs("/", &stat) != 0) { // 呼叫 statvfs() 取得檔案系統統計資訊
        std::cerr << "Failed to get disk usage information" << std::endl; // 輸出錯誤訊息
        return -1.0; // 返回 -1.0 表示取得磁碟使用情況失敗
    }
    double totalSpace = stat.f_blocks * stat.f_frsize / (1024.0 * 1024.0 * 1024.0); // 計算總共的磁碟空間 (單位: GB)
    double freeSpace = stat.f_bfree * stat.f_frsize / (1024.0 * 1024.0 * 1024.0);   // 計算剩餘的磁碟空間 (單位: GB)
    return totalSpace - freeSpace; // 返回已使用的磁碟空間
}

// 取得網路吞吐量 (假設監控 en0 介面)
double getNetworkUsage() {
    struct ifaddrs *ifaddr, *ifa; // 宣告兩個指標變數，用來指向網路介面地址結構體
    if (getifaddrs(&ifaddr) == -1) { // 呼叫 getifaddrs() 取得網路介面地址資訊
        std::cerr << "Failed to get network usage information" << std::endl; // 輸出錯誤訊息
        return -1.0; // 返回 -1.0 表示取得網路使用情況失敗
    }

    double dataSent = 0.0, dataReceived = 0.0; // 宣告兩個變數，用來存放網路傳輸和接收的資料量
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) { // 迴圈處理每個網路介面
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_LINK && std::string(ifa->ifa_name) == "en0") { // 判斷是否為 en0 介面
            struct if_data *ifData = (struct if_data *)ifa->ifa_data; // 取得網路介面資料
            dataSent = ifData->ifi_obytes / (1024.0 * 1024.0); // 計算網路傳輸的資料量 (單位: MB)
            dataReceived = ifData->ifi_ibytes / (1024.0 * 1024.0); // 計算網路接收的資料量 (單位: MB)
        }
    }
    freeifaddrs(ifaddr); // 釋放網路介面地址資訊
    return dataSent + dataReceived; // 返回網路吞吐量
}
//get cpu temerature
double getCpuTemperature() {
    std::string command = "ioreg -c IOHWSensor | grep -i temperature | awk '{print $NF}' | head -n 1"; // 組合 shell 命令，用來取得 CPU 溫度
    FILE* pipe = popen(command.c_str(), "r"); // 執行 shell 命令，並開啟管道
    if (!pipe) { // 判斷管道是否開啟成功
        std::cerr << "Failed to get CPU temperature" << std::endl; // 輸出錯誤訊息
        return -1.0; // 返回 -1.0 表示取得 CPU 溫度失敗
    }

    char buffer[128]; // 宣告一個字元陣列，用來存放管道輸出的資料
    std::string result = ""; // 宣告一個字串變數，用來存放 CPU 溫度
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) { // 讀取管道輸出的資料
        result += buffer; // 將管道輸出的資料存放到 result 變數中
    }
    pclose(pipe); // 關閉管道

    // 將結果轉換為浮點數
    try {
        return std::stod(result) / 100.0; // 部分系統返回的單位是分攝氏
    } catch (...) {
        std::cerr << "Failed to parse CPU temperature" << std::endl; // 輸出錯誤訊息
        return -1.0; // 返回 -1.0 表示轉換 CPU 溫度失敗
    }
}

// get per core usage
std::vector<double> getCoreUsage() {
    std::string command = "top -l 1 -n 0 | grep 'CPU usage'"; // 組合 shell 命令，用來取得每個核心的使用率
    FILE* pipe = popen(command.c_str(), "r"); // 執行 shell 命令，並開啟管道
    if(!pipe) { // 判斷管道是否開啟成功
        std::cerr << "Failed to get CPU core usage" << std::endl; // 輸出錯誤訊息
        return std::vector<double>(); // 返回空的向量，表示取得每個核心的使用率失敗
    }
    char buffer[128]; // 宣告一個字元陣列，用來存放管道輸出的資料
    std::string result = ""; // 宣告一個字串變數，用來存放每個核心的使用率
    while(fgets(buffer, sizeof(buffer), pipe) != nullptr) { // 讀取管道輸出的資料
        result += buffer; // 將管道輸出的資料存放到 result 變數中
    }
    pclose(pipe); // 關閉管道

    std::stringstream ss(result); // 建立一個字串流，用來解析 result 變數中的資料
    std::string token; // 宣告一個字串變數，用來存放解析出來的資料
    std::vector<double> coreUsage; // 宣告一個 double 型態的向量，用來存放每個核心的使用率
    while (ss >> token) { // 迴圈解析 result 變數中的資料
        // Filtering out non-numeric tokens
        if (token.find('%') != std::string::npos) { // 判斷 token 是否包含 '%' 字元
            token.erase(token.find('%')); // 移除 token 中的 '%' 字元
            try {
                coreUsage.push_back(std::stod(token)); // 將 token 轉換為 double 型態，並存放到 coreUsage 向量中
            } catch (const std::invalid_argument& e) { // 捕捉轉換失敗的例外
                std::cerr << "Failed to parse core usage: " << e.what() << std::endl; // 輸出錯誤訊息
                return std::vector<double>(); // 返回空的向量，表示解析每個核心的使用率失敗
            }
        }
    }
    return coreUsage; // 返回每個核心的使用率
}
// std::string getDiskHealth() {
//     std::string command = "smartctl -H /dev/disk0 | grep 'SMART Health Status'";
//     FILE* pipe = popen(command.c_str(), "r");
//     if (!pipe) {
//         std::cerr << "Failed to get disk health status" << std::endl;
//         return "Unknown";
//     }

//     char buffer[128];
//     std::string result = "";
//     while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
//         result += buffer;
//     }
//     pclose(pipe);
//     return result;
// }


// CPU 和記憶體監控執行緒
void monitorSystemUsage() {
    while (keepRunning) { // 迴圈執行，直到 keepRunning 變數為 false
        std::vector<double> coreUsages = getCoreUsage(); // 取得每個核心的使用率
        double cpuUsage = getCpuUsage(); // 取得 CPU 使用率
        double memoryUsage = getMemoryUsage(); // 取得記憶體使用量
        double diskUsage = getDiskUsage(); // 取得磁碟使用量
        double networkUsage = getNetworkUsage(); // 取得網路使用量
        double cpuTemperature = getCpuTemperature(); // 取得 CPU 溫度
    
        // 顯示每個核心的使用率
        std::cout << "Core Usage: ";
        for (const auto& usage : coreUsages) { // 迴圈輸出每個核心的使用率
            std::cout << usage << "% ";
        }
        std::cout << std::endl;

        if (cpuUsage >= 0) { // 判斷 CPU 使用率是否有效
            std::cout << "CPU Usage: " << cpuUsage << "%" << std::endl; // 輸出 CPU 使用率
        }
        if (memoryUsage >= 0) { // 判斷記憶體使用量是否有效
            std::cout << "Memory Usage: " << memoryUsage << " MB" << std::endl; // 輸出記憶體使用量
        }
        if (diskUsage >= 0) { // 判斷磁碟使用量是否有效
            std::cout << "Disk Usage: " << diskUsage << " GB" << std::endl; // 輸出磁碟使用量
        }
        if (networkUsage >= 0) { // 判斷網路使用量是否有效
            std::cout << "Network Usage: " << networkUsage << " MB" << std::endl; // 輸出網路使用量
        }
        if (cpuTemperature >= 0) { // 判斷 CPU 溫度是否有效
            std::cout << "CPU Temperature: " << cpuTemperature << " °C" << std::endl; // 輸出 CPU 溫度
        }
        std::cout << "-----------------------------" << std::endl; // 輸出分隔線
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 暫停 1 秒
    }
}

// 監控按鍵輸入的執行緒，用於終止程式
void waitForExit() {
    std::cout << "Press 'q' to stop the program..." << std::endl; // 輸出提示訊息
    char input; // 宣告一個字元變數，用來存放使用者輸入的字元
    while (keepRunning) { // 迴圈執行，直到 keepRunning 變數為 false
        if (std::cin >> input && (input == 'q' || input == 'Q')) { // 判斷使用者是否輸入 'q' 或 'Q'
            keepRunning = false; // 設定 keepRunning 變數為 false，終止程式
        }
    }
}

int main() {
    // 啟動 CPU 和記憶體監控執行緒
    std::thread monitorThread(monitorSystemUsage); // 建立一個執行緒，執行 monitorSystemUsage() 函式
    // 啟動監控按鍵輸入的執行緒
    std::thread exitThread(waitForExit); // 建立一個執行緒，執行 waitForExit() 函式

    // 等待執行緒結束
    exitThread.join(); // 等待 waitForExit() 執行緒結束
    keepRunning = false; // 若退出輸入偵聽，強制關閉 monitorSystemUsage 執行緒
    monitorThread.join(); // 等待 monitorSystemUsage() 執行緒結束

    std::cout << "Program terminated." << std::endl; // 輸出程式結束訊息
    return 0; // 返回 0，表示程式正常結束
}
