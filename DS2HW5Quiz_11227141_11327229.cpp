//11227141 鍾博竣
//11327229 游啓揚

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
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

// 輔助索引（Secondary Index）項目結構，保存同一位發訊者在排序檔中的所有 record offset
struct SecondaryIndexEntry {
    string senderID;          // 發訊者學號 (putID)
    vector<int> offsets;      // 該發訊者所有符合範圍條件的資料位移量
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
    int bufferSize;   // 使用者指定的緩衝區大小，範圍限制為 [300, 60000]
    vector<IndexEntry> primaryIndex;  // 任務二建立的主索引，供任務三依權重範圍定位使用
    vector<SecondaryIndexEntry> secondaryIndex;  // 任務三建立的輔助索引，供任務四查詢使用

    /**
     * @brief 將 char 陣列形式的學號轉成乾淨的 string
     *
     * 二進位檔中的學號欄位長度固定為 10 bytes，實際字串可能在 '\0' 後仍殘留填充資料，
     * 因此必須只擷取 '\0' 前的有效內容，避免輸出出現亂碼。
     */
    string idToString(const char id[10]) {
        return string(id, strnlen(id, 10));
    }

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

        int inputBufferSize = max(1, bufferSize / 3);
        int outputBufferSize = max(1, bufferSize - 2 * inputBufferSize);

        vector<Record> buffer1(inputBufferSize);
        vector<Record> buffer2(inputBufferSize);
        vector<Record> bufferOut(outputBufferSize);

        int ptr1 = 0, count1 = 0;
        int ptr2 = 0, count2 = 0;
        int outCount = 0;

        // Lambda 封裝：當快取一被消耗完時，自動從 file1 批次讀取至多 100 筆新資料
        auto getRecord1 = [&]() -> Record* {
            if (ptr1 >= count1) {
                f1.read(reinterpret_cast<char*>(buffer1.data()), inputBufferSize * sizeof(Record));
                count1 = f1.gcount() / sizeof(Record);
                ptr1 = 0;
            }
            if (count1 == 0) return nullptr; // 檔案結束 (EOF)
            return &buffer1[ptr1];
        };

        // Lambda 封裝：當快取二被消耗完時，自動從 file2 批次讀取至多 100 筆新資料
        auto getRecord2 = [&]() -> Record* {
            if (ptr2 >= count2) {
                f2.read(reinterpret_cast<char*>(buffer2.data()), inputBufferSize * sizeof(Record));
                count2 = f2.gcount() / sizeof(Record);
                ptr2 = 0;
            }
            if (count2 == 0) return nullptr; // 檔案結束 (EOF)
            return &buffer2[ptr2];
        };

        // Lambda 封裝：將單筆紀錄寫入輸出快取，滿 100 筆時一次性沖刷寫入磁碟，大幅減少 IO 次數
        auto writeRecord = [&](const Record& rec) {
            bufferOut[outCount++] = rec;
            if (outCount == outputBufferSize) {
                fOut.write(reinterpret_cast<const char*>(bufferOut.data()), outputBufferSize * sizeof(Record));
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
    ExternalSorter(const string& num, int bufSize) : fileNum(num), bufferSize(bufSize) {}

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
        vector<Record> memBuffer(bufferSize); // 內部排序記憶體預算限制：使用者指定的 bufferSize 筆

        // Step 1: 劃分初始區塊並寫出 initial sorted runs
        while (true) {
            inFile.read(reinterpret_cast<char*>(memBuffer.data()), bufferSize * sizeof(Record));
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

        primaryIndex.clear();
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

    /**
     * @brief 確保主索引已存在，任務三必須依照任務二建立的主索引進行範圍搜尋
     */
    void ensurePrimaryIndex() {
        if (!primaryIndex.empty()) return;

        string sortedOutName = "order" + fileNum + ".bin";
        ifstream f(sortedOutName, ios::binary);
        if (!f.is_open()) return;

        primaryIndex.clear();
        Record rec;
        int recordIndex = 0;
        float lastWeight = -1.0f;

        // 此處只補建記憶體索引，不額外印出任務二的畫面，避免破壞任務三輸出格式
        while (f.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
            if (primaryIndex.empty() || rec.weight != lastWeight) {
                primaryIndex.push_back({rec.weight, recordIndex});
                lastWeight = rec.weight;
            }
            recordIndex++;
        }
        f.close();
    }

    /**
     * @brief 任務三：依使用者輸入的量化權重範圍建立輔助索引
     *
     * 透過任務二的主索引先定位可能符合範圍的連續 record offset，再以小批次緩衝區
     * 循序讀取排序檔，將符合條件的 putID 累積到輔助索引中。
     */
    bool buildSecondaryIndexByRange(float value1, float value2) {
        ensurePrimaryIndex();
        secondaryIndex.clear();

        string sortedOutName = "order" + fileNum + ".bin";
        ifstream f(sortedOutName, ios::binary);
        if (!f.is_open()) return false;

        float high = max(value1, value2);
        float low = min(value1, value2);
        int startOffset = -1;
        int endOffset = -1;

        // 由主索引找出第一個可能符合 high >= weight >= low 的資料區間起點
        for (size_t i = 0; i < primaryIndex.size(); ++i) {
            if (primaryIndex[i].weight <= high) {
                startOffset = primaryIndex[i].recordOffset;
                break;
            }
        }

        if (startOffset == -1) {
            cout << "There are 0 records in total.\n";
            cout << "There are 0 senders in total.\n";
            f.close();
            return true;
        }

        // 由主索引找出第一個小於 low 的權重所在位置，作為本次範圍搜尋的結束位置
        f.seekg(0, ios::end);
        int totalRecords = static_cast<int>(f.tellg() / sizeof(Record));
        endOffset = totalRecords;
        for (size_t i = 0; i < primaryIndex.size(); ++i) {
            if (primaryIndex[i].recordOffset > startOffset && primaryIndex[i].weight < low) {
                endOffset = primaryIndex[i].recordOffset;
                break;
            }
        }

        map<string, vector<int> > tempIndex;
        vector<Record> buffer(bufferSize); // 任務三分批讀入排序檔，避免一次載入整個檔案
        int currentOffset = startOffset;
        int totalMatched = 0;

        f.seekg(static_cast<long long>(startOffset) * sizeof(Record), ios::beg);
        while (currentOffset < endOffset) {
            int want = min(bufferSize, endOffset - currentOffset);
            f.read(reinterpret_cast<char*>(buffer.data()), want * sizeof(Record));
            int got = static_cast<int>(f.gcount() / sizeof(Record));
            if (got == 0) break;

            for (int i = 0; i < got; ++i) {
                if (buffer[i].weight <= high && buffer[i].weight >= low) {
                    tempIndex[idToString(buffer[i].putID)].push_back(currentOffset + i);
                    totalMatched++;
                }
            }
            currentOffset += got;
        }
        f.close();

        for (map<string, vector<int> >::iterator it = tempIndex.begin(); it != tempIndex.end(); ++it) {
            secondaryIndex.push_back({it->first, it->second});
        }

        cout << "There are " << totalMatched << " records in total.\n";
        cout << "There are " << secondaryIndex.size() << " senders in total.\n";
        for (size_t i = 0; i < secondaryIndex.size(); ++i) {
            cout << "[" << setw(4) << i + 1 << "]   "
                 << secondaryIndex[i].senderID << "\t"
                 << setw(5) << secondaryIndex[i].offsets.size() << "\n";
        }

        return true;
    }

    /**
     * @brief 任務四：使用任務三建立的輔助索引查詢指定發訊者的所有資料
     */
    void searchSenderBySecondaryIndex(const string& senderID) {
        int found = -1;
        for (size_t i = 0; i < secondaryIndex.size(); ++i) {
            if (secondaryIndex[i].senderID == senderID) {
                found = static_cast<int>(i);
                break;
            }
        }

        if (found == -1) {
            cout << "Sender " << senderID << " does not exist.\n";
            return;
        }

        string sortedOutName = "order" + fileNum + ".bin";
        ifstream f(sortedOutName, ios::binary);
        if (!f.is_open()) return;

        cout << "Sender " << senderID << " has "
             << secondaryIndex[found].offsets.size() << " records.\n";

        Record rec;
        for (size_t i = 0; i < secondaryIndex[found].offsets.size(); ++i) {
            f.seekg(static_cast<long long>(secondaryIndex[found].offsets[i]) * sizeof(Record), ios::beg);
            f.read(reinterpret_cast<char*>(&rec), sizeof(Record));
            cout << "[" << setw(3) << i + 1 << "]"
                 << setw(11) << idToString(rec.getID)
                 << setw(12) << fixed << setprecision(2) << rec.weight << "\n";
        }
        f.close();
        cout.unsetf(ios::floatfield);
        cout << setprecision(6);
    }

    /**
     * @brief 任務三與任務四的整合互動流程
     */
    void executeRangeSearchAndQuery() {
        while (true) {
            cout << "##################################\n"
                 << "* 3: Range search to build index *\n"
                 << "##################################\n\n";

            cout << "Input two values in (0,1] for range search.\n\n";

            float first = 0.0f;
            float second = 0.0f;
            string line = "";

            while (true) {
                cout << "Input a floating number in [0.01, 1]: ";
                if (!getline(cin, line)) return;
                stringstream ss(line);
                if ((ss >> first) && first >= 0.01f && first <= 1.00f) break;
                cout << "\n### It is NOT in [0.01,1] ###\n\n";
            }

            cout << "\n";
            while (true) {
                cout << "Input a floating number in [0.01, 1]: ";
                if (!getline(cin, line)) return;
                stringstream ss(line);
                if ((ss >> second) && second >= 0.01f && second <= 1.00f) break;
                cout << "\n### It is NOT in [0.01,1] ###\n\n";
            }

            cout << "\n";
            if (!buildSecondaryIndexByRange(first, second)) return;

            cout << "\n";
            while (true) {
                cout << "Input a student ID ([4] Quit): ";
                if (!getline(cin, line)) return;
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line == "4") { cout << "\n"; break; }

                searchSenderBySecondaryIndex(line);
                cout << "\n";
            }

            cout << "[3]Quit or [Any other key]continue?\n";
            if (!getline(cin, line)) return;
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line == "3") break;

            cout << "\n";
        }
    }
};

/**
 * @brief 程式主流程控制
 *
 * 實作使用者互動、防呆檔名檢查與接續/結束程式的控制邏輯。
 */
int main() {
    int bufferSize = 300;

    while (true) {
        // 印出標準首選單與參數配置
        cout << "* Data Structures and Algorithms *\n"
             << "**********************************\n"
             << "* 1. External merge sort on file *\n"
             << "* 2: Construct the primary index *\n"
             << "* 3: Range search to build index *\n"
             << "* 4: Retrieve records from index *\n"
             << "**********************************\n"
             << "*** The buffer size is " << bufferSize << "\n";

        string bufferLine = "";
        while (true) {
            cout << "Input a new buffer size in [300, 60000]: ";
            if (!getline(cin, bufferLine)) {
                return 0;
            }
            if (!bufferLine.empty() && bufferLine.back() == '\r') {
                bufferLine.pop_back();
            }

            stringstream ss(bufferLine);
            if ((ss >> bufferSize) && bufferSize >= 300 && bufferSize <= 60000) {
                break;
            }
            cout << "### Input is NOT in [300, 60000] ###\n";
        }
        cout << "\n";

        // Mission 1 標頭只在進入此輪檔案處理時印出一次，不置於內層輸入防呆迴圈內
        cout << "##################################\n"
             << "* 1. External merge sort on file *\n"
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

        ExternalSorter sorter(fileNum, bufferSize);
        double tInternal = 0.0, tExternal = 0.0;

        // 執行排序與主索引建置
        if (sorter.executeSort(tInternal, tExternal)) {
            double tTotal = tInternal + tExternal;
            
            // 使用局部獨立的 stringstream 格式化執行時間（保留3位小數），防止污染 cout 的全域浮點輸出格式
            stringstream ssInternal, ssExternal, ssTotal;
            ssInternal << fixed << setprecision(2) << tInternal;
            ssExternal << fixed << setprecision(2) << tExternal;
            ssTotal << fixed << setprecision(2) << tTotal;

            cout << "The execution time ...\n"
                 << "Internal Sort = " << ssInternal.str() << " ms\n"
                 << "External Sort = " << ssExternal.str() << " ms\n"
                 << "Total Execution Time = " << ssTotal.str() << " ms\n\n"
                 << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                 << "* 2: Construct the primary index *\n"
                 << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n\n";

            sorter.buildPrimaryIndex();
            cout << "\n";
            sorter.executeRangeSearchAndQuery();
        }

        // 成功執行完四大任務後，進行循環詢問（帶前導換行）
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
