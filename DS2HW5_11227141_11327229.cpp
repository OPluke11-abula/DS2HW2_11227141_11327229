/**
 * @file DS2HW5_11227141_11327229.cpp
 * @brief 資料結構與演算法 - 作業五：外部排序與主索引建置
 * 
 * 本程式主要實作以下兩大任務：
 * 1. 任務一 (Mission 1)：外部歸併排序 (External Merge Sort)
 *    - 將大於記憶體限制的二進位原始資料進行分批排序。
 *    - 記憶體快取上限固定為 300 筆紀錄 (Record)。
 *    - 實作高度穩定的 2-Way Merge (雙路歸併)，在權重相等時嚴格保留原始次序。
 *    - 合併完成後，程式會自動清理磁碟中的所有臨時過渡暫存檔 (temp_pass_*)。
 * 2. 任務二 (Mission 2)：建立主索引 (Primary Index)
 *    - 讀取已排序的二進位檔案，建置記憶體中的主索引表。
 *    - 針對各個不重複的浮點數權重鍵值，記錄其首筆紀錄的 0-based 資料序號偏移量。
 * 
 * @author 11227141 鍾博竣
 * @author 11327229 游啓揚
 * 
 * @note 編譯與執行指令：
 *       g++ -O3 -std=c++17 DS2HW5_11227141_11327229.cpp -o DS2HW5
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// 強制資料結構體以 1 位元組對齊，確保 Record 大小精確為 24 位元組 (10 + 10 + 4)，防止編譯器填充空白位元組
#pragma pack(push, 1)
struct Record {
    char putID[10];  // 發訊學生的學號陣列
    char getID[10];  // 收訊學生的學號陣列
    float weight;    // 訊息量量化權重（主要排序鍵值，遞減排序）
};
#pragma pack(pop)

// 主索引（Primary Index）項目結構，保存不重複的權重鍵值與檔案位址（0-based 筆數偏移量）
struct IndexEntry {
    float weight;      // 索引鍵值 (Key)
    int recordOffset;  // 檔案內紀錄序號 (Offset)
};

/**
 * @class ExternalSorter
 * @brief 外部排序與索引管理類別
 * 
 * 封裝外部排序、檔案歸併、主索引建置等核心邏輯，避免全域變數污染，提高物件的模組化與可維護性。
 */
class ExternalSorter {
private:
    string fileNum;  // 目標檔案的編號（如 501，供建置pairs*.bin與order*.bin檔名使用）

    /**
     * @brief 生成臨時合併暫存檔（Run File）的名稱
     * @param pass 當前合併階層（0 代表初始排序生成的子區塊）
     * @param runIndex 當前階層的第幾個子區塊
     * @return 臨時檔名字串
     */
    string getRunName(int pass, int runIndex) {
        return "temp_pass_" + to_string(pass) + "_" + to_string(runIndex) + ".bin";
    }

    /**
     * @brief 實作雙路歸併（2-Way Merge）演算法，兩兩合併局部排序檔
     * 
     * 本方法嚴格遵守記憶體配額限制，在合併過程中維持 300 筆 Record 的總快取上限：
     *  - buffer1: 100 筆 (快取第一個輸入檔)
     *  - buffer2: 100 筆 (快取第二個輸入檔)
     *  - bufferOut: 100 筆 (快取寫出到輸出檔，滿 100 筆即進行一次實體磁碟寫入)
     * 
     * @param file1 第一個已排序的局部暫存檔路徑
     * @param file2 第二個已排序的局部暫存檔路徑
     * @param outFile 合併後的新暫存檔路徑
     */
    void mergeRuns(const string& file1, const string& file2, const string& outFile) {
        ifstream f1(file1, ios::binary);
        ifstream f2(file2, ios::binary);
        ofstream fOut(outFile, ios::binary);

        vector<Record> buffer1(100);
        vector<Record> buffer2(100);
        vector<Record> bufferOut(100);

        int ptr1 = 0, count1 = 0;
        int ptr2 = 0, count2 = 0;
        int outCount = 0;

        // Lambda 封裝：當快取一被消耗完時，自動從 file1 批次讀取至多 100 筆新資料
        auto getRecord1 = [&]() -> Record* {
            if (ptr1 >= count1) {
                f1.read(reinterpret_cast<char*>(buffer1.data()), 100 * sizeof(Record));
                count1 = f1.gcount() / sizeof(Record);
                ptr1 = 0;
            }
            if (count1 == 0) return nullptr; // 檔案結束 (EOF)
            return &buffer1[ptr1];
        };

        // Lambda 封裝：當快取二被消耗完時，自動從 file2 批次讀取至多 100 筆新資料
        auto getRecord2 = [&]() -> Record* {
            if (ptr2 >= count2) {
                f2.read(reinterpret_cast<char*>(buffer2.data()), 100 * sizeof(Record));
                count2 = f2.gcount() / sizeof(Record);
                ptr2 = 0;
            }
            if (count2 == 0) return nullptr; // 檔案結束 (EOF)
            return &buffer2[ptr2];
        };

        // Lambda 封裝：將單筆紀錄寫入輸出快取，滿 100 筆時一次性沖刷寫入磁碟，大幅減少 IO 次數
        auto writeRecord = [&](const Record& rec) {
            bufferOut[outCount++] = rec;
            if (outCount == 100) {
                fOut.write(reinterpret_cast<const char*>(bufferOut.data()), 100 * sizeof(Record));
                outCount = 0;
            }
        };

        Record* r1 = getRecord1();
        Record* r2 = getRecord2();

        // 雙指針線性歸併掃描
        while (r1 != nullptr || r2 != nullptr) {
            if (r1 != nullptr && r2 != nullptr) {
                if (r1->weight > r2->weight) {
                    writeRecord(*r1);
                    ptr1++;
                    r1 = getRecord1();
                } else if (r1->weight < r2->weight) {
                    writeRecord(*r2);
                    ptr2++;
                    r2 = getRecord2();
                } else {
                    // 【關鍵穩定排序控制】：當權重相等時，優先寫入來自第一個 Run 檔案的資料。
                    // 由於第一個 Run 在原始檔案的位置較前，如此可確保 Stable Sort 的要求。
                    writeRecord(*r1);
                    ptr1++;
                    r1 = getRecord1();
                }
            } else if (r1 != nullptr) {
                writeRecord(*r1);
                ptr1++;
                r1 = getRecord1();
            } else {
                writeRecord(*r2);
                ptr2++;
                r2 = getRecord2();
            }
        }

        // 將最後殘留於快取中不足 100 筆的剩餘資料沖刷寫入硬碟
        if (outCount > 0) {
            fOut.write(reinterpret_cast<const char*>(bufferOut.data()), outCount * sizeof(Record));
        }

        f1.close();
        f2.close();
        fOut.close();
    }

public:
    /**
     * @brief 建構管理器實例
     * @param num 檔案編號 (例如 "501")
     */
    ExternalSorter(const string& num) : fileNum(num) {}

    /**
     * @brief 執行任務一：外部合併排序
     * 
     * 步驟：
     * 1. 劃分初始排序區塊 (Internal Sort)：
     *    - 每次以 300 筆為單位讀入 pairs*.bin。
     *    - 呼叫 std::stable_sort 降序排序後，寫出為 Pass-0 的多個 temp_pass_0_*.bin。
     * 2. 階段式歸併 (External Sort)：
     *    - 兩兩合併相鄰的暫存檔，並在合併完成後立刻調用 std::remove 刪除舊檔案以維持硬碟整潔。
     *    - 當遇上奇數檔案時，直接將該孤立暫存檔重命名移交至下一階段。
     * 3. 輸出正式排序檔案 order*.bin 並回傳高精度計時。
     * 
     * @param[out] tInternal 內部局部排序產生成本 (毫秒)
     * @param[out] tExternal 多 Pass 遞迴合併磁碟 I/O 成本 (毫秒)
     * @return true 執行成功且檔案已生成
     * @return false 原始 pairs*.bin 檔案不存在 (防呆阻斷)
     */
    bool executeSort(double& tInternal, double& tExternal) {
        auto tStart = chrono::high_resolution_clock::now();
        string inName = "pairs" + fileNum + ".bin";
        ifstream inFile(inName, ios::binary);
        if (!inFile.is_open()) return false;

        int runIndex = 0;
        vector<Record> memBuffer(300); // 內部排序記憶體預算限制：300 筆

        // Step 1: 劃分初始區塊並寫出 initial sorted runs
        while (true) {
            inFile.read(reinterpret_cast<char*>(memBuffer.data()), 300 * sizeof(Record));
            int countRead = inFile.gcount() / sizeof(Record);
            if (countRead == 0) break;

            // 僅對實際讀取到的範圍建立 activeBlock 進行排序，防止溢出或未填充數據參雜
            vector<Record> activeBlock(memBuffer.begin(), memBuffer.begin() + countRead);
            stable_sort(activeBlock.begin(), activeBlock.end(), [](const Record& a, const Record& b) {
                return a.weight > b.weight;
            });

            string runName = getRunName(0, runIndex);
            ofstream runFile(runName, ios::binary);
            runFile.write(reinterpret_cast<const char*>(activeBlock.data()), countRead * sizeof(Record));
            runFile.close();
            runIndex++;
        }
        inFile.close();

        auto tInternalEnd = chrono::high_resolution_clock::now();
        cout << "\nThe internal sort is completed. Check the initial sorted runs! \n\n"
             << "Now there are " << runIndex << " runs.\n\n";

        // Step 2: 遞迴式 Pass-by-Pass 二路歸併
        int currentPassRuns = runIndex;
        int pass = 0;

        while (currentPassRuns > 1) {
            int nextPassRuns = 0;
            for (int i = 0; i < currentPassRuns; i += 2) {
                if (i + 1 < currentPassRuns) {
                    string r1 = getRunName(pass, i);
                    string r2 = getRunName(pass, i + 1);
                    string rOut = getRunName(pass + 1, nextPassRuns);

                    mergeRuns(r1, r2, rOut);
                    std::remove(r1.c_str()); // 立即刪除已合併完畢的舊過渡暫存檔，保護硬碟空間
                    std::remove(r2.c_str());
                    nextPassRuns++;
                } else {
                    // 孤立未配對的單一暫存檔直接重新命名，升格移交至下一階段
                    string r1 = getRunName(pass, i);
                    string rOut = getRunName(pass + 1, nextPassRuns);
                    std::rename(r1.c_str(), rOut.c_str());
                    nextPassRuns++;
                }
            }
            pass++;
            currentPassRuns = nextPassRuns;
            cout << "Now there are " << currentPassRuns << " runs.\n\n";
        }

        // Step 3: 將最後的單一歸併檔案重命名為 order*.bin，完成外部排序
        string finalTempRun = getRunName(pass, 0);
        string sortedOutName = "order" + fileNum + ".bin";
        std::remove(sortedOutName.c_str()); // 防止舊有的殘留排序檔干擾重命名操作
        std::rename(finalTempRun.c_str(), sortedOutName.c_str());

        auto tExternalEnd = chrono::high_resolution_clock::now();
        tInternal = chrono::duration<double, milli>(tInternalEnd - tStart).count();
        tExternal = chrono::duration<double, milli>(tExternalEnd - tInternalEnd).count();
        
        return true;
    }

    /**
     * @brief 任務二：讀取已排序檔案，建置記憶體主索引表並以指定格式輸出
     * 
     * 本方法分批掃描檔案，在發現權重邊界改變（或首筆資料）時，
     * 向主索引表中添加 (weight, recordOffset) 對，隨後輸出至螢幕。
     */
    void buildPrimaryIndex() {
        string sortedOutName = "order" + fileNum + ".bin";
        ifstream f(sortedOutName, ios::binary);
        if (!f.is_open()) return;

        vector<IndexEntry> primaryIndex;
        Record rec;
        int recordIndex = 0;
        float lastWeight = -1.0f;

        // 循序遍歷整檔，擷取不重複權重的首筆資料序號
        while (f.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
            if (primaryIndex.empty() || rec.weight != lastWeight) {
                primaryIndex.push_back({rec.weight, recordIndex});
                lastWeight = rec.weight;
            }
            recordIndex++;
        }
        f.close();

        // 精確匹配 DEMO 中的主索引輸出格式 (key, offset)
        cout << "<Primary index>: (key, offset)\n";
        for (size_t i = 0; i < primaryIndex.size(); ++i) {
            cout << "[" << i + 1 << "] (" << primaryIndex[i].weight << ", " << primaryIndex[i].recordOffset << ")\n";
        }
    }
};

/**
 * @brief 程式主流程控制
 * 
 * 實作使用者互動、防呆檔名檢查與接續/結束程式的控制邏輯。
 */
int main() {
    while (true) {
        // 印出標準首選單與參數配置
        cout << "* Data Structures and Algorithms *\n"
             << "**********************************\n"
             << "* 1. External merge sort on file *\n"
             << "* 2: Construct the primary index *\n"
             << "**********************************\n"
             << "*** The buffer size is 300\n";

        // Mission 1 標頭只在進入此輪檔案處理時印出一次，不置於內層輸入防呆迴圈內
        cout << "##################################\n"
             << "Mission 1: External merge sort \n"
             << "##################################\n\n";

        string fileNum = "";
        while (true) {
            cout << "Input the file name: [0]Quit\n";

            if (!getline(cin, fileNum)) {
                fileNum = "0";
                break;
            }
            // 過濾可能的跨平台 Windows 回車字元 (\r)
            if (!fileNum.empty() && fileNum.back() == '\r') {
                fileNum.pop_back();
            }
            if (fileNum == "0") {
                break;
            }

            // 防呆驗證：嘗試唯讀開啟原始 pairs*.bin 檔案，若開啟失敗代表檔案不存在，觸發重試
            string testName = "pairs" + fileNum + ".bin";
            ifstream testFile(testName, ios::binary);
            if (testFile.is_open()) {
                testFile.close();
                break; // 檔案存在，跳出防呆輸入迴圈
            } else {
                cout << "\npairs" << fileNum << ".bin does not exist!!!\n\n";
            }
        }

        // 若使用者輸入 0 放棄執行，觸發退出詢問，符合測資答案的空行格式
        if (fileNum == "0") {
            cout << "\n[0]Quit or [Any other key]continue?\n";
            string cont = "";
            if (!getline(cin, cont)) {
                break;
            }
            if (!cont.empty() && cont.back() == '\r') {
                cont.pop_back();
            }
            if (cont == "0") {
                break;
            }
            cout << "\n";
            continue;
        }

        ExternalSorter sorter(fileNum);
        double tInternal = 0.0, tExternal = 0.0;

        // 執行排序與主索引建置
        if (sorter.executeSort(tInternal, tExternal)) {
            double tTotal = tInternal + tExternal;
            
            // 使用局部獨立的 stringstream 格式化執行時間（保留3位小數），防止污染 cout 的全域浮點輸出格式
            stringstream ssInternal, ssExternal, ssTotal;
            ssInternal << fixed << setprecision(3) << tInternal;
            ssExternal << fixed << setprecision(3) << tExternal;
            ssTotal << fixed << setprecision(3) << tTotal;

            cout << "The execution time ...\n"
                 << "Internal Sort = " << ssInternal.str() << " ms\n"
                 << "External Sort = " << ssExternal.str() << " ms\n"
                 << "Total Execution Time = " << ssTotal.str() << " ms\n\n"
                 << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                 << "Mission 2: Build the primary index \n"
                 << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n\n";

            sorter.buildPrimaryIndex();
        }

        // 成功執行完兩大任務後，進行循環詢問（帶前導換行）
        cout << "\n[0]Quit or [Any other key]continue?\n";
        string action = "";
        if (!getline(cin, action)) {
            break;
        }
        if (!action.empty() && action.back() == '\r') {
            action.pop_back();
        }
        if (action == "0") {
            break;
        }
        cout << "\n";
    }

    return 0;
}
