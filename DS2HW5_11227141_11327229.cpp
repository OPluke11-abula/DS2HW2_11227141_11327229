// 11227141 鍾博竣 // 註解：第一位作者學號與姓名
// 11327229 游啓揚 // 註解：第二位作者學號與姓名
#include <algorithm> // 註解：引入標準算法庫，主要使用穩定排序演算法 stable_sort
#include <chrono> // 註解：引入時間度量庫，用於高精度評估執行時間
#include <cstdio> // 註解：引入 C 系統庫，調用 std::remove 及 std::rename 進行暫存檔生命週期管理
#include <fstream> // 註解：引入檔案輸入輸出流，提供高效 binary file streams 的磁碟讀寫功能
#include <iomanip> // 註解：引入輸入輸出操作器庫，用於自定義控制台的流格式
#include <iostream> // 註解：引入標準輸入輸出流庫，與控制台進行系統級互動
#include <sstream> // 註解：引入字串流庫，用於獨立格式化浮點數時間，不干擾 cout 的預設全域狀態
#include <string> // 註解：引入字串庫，用於檔名組合與命令列輸入處理
#include <vector> // 註解：引入向量容器庫，用於分批資料快取與記憶體緩衝區配置
using namespace std; // 註解：使用標準命名空間，減少冗餘命名限定符
#pragma pack(push, 1) // 註解：以 1 位元組對齊封裝資料結構，避免編譯器因對齊補零而偏離 24 位元組的硬性大小限制
struct Record { // 註解：定義二進位資料實體結構
    char putID[10]; // 註解：發訊者學號資訊（10 位元組的字元陣列，內含原始位元組編碼）
    char getID[10]; // 註解：收訊者學號資訊（10 位元組的字元陣列，內含原始位元組編碼）
    float weight; // 註解：量化互動權重，4 位元組單精度浮點數，主要排序欄位
}; // 註解：結束封裝資料實體結構
#pragma pack(pop) // 註解：恢復編譯器的預設記憶體對齊設定
struct IndexEntry { // 註解：定義記憶體中主索引（Primary Index）之節點結構
    float weight; // 註解：鍵值（Key）：排序欄位 weight，代表該區間之代表權重
    int recordOffset; // 註解：檔案位址（Offset）：該權重區段首筆紀錄在檔案中從 0 開始之序號
}; // 註解：結束主索引節點結構體定義
class ExternalSorter { // 註解：定義外部排序與索引管理器類別，落實物件導向封裝
private: // 註解：私有成員區段，保護關鍵系統方法與變數
    string fileNum; // 註解：當前操作的目標測試檔案流水編號字串
    string getRunName(int pass, int runIndex) { // 註解：依據合併階段與子區塊生成暫存檔名
        return "temp_pass_" + to_string(pass) + "_" + to_string(runIndex) + ".bin"; // 註解：建構臨時的二進位階段檔名
    } // 註解：結束檔名建構私有方法
    void mergeRuns(const string& file1, const string& file2, const string& outFile) { // 註解：對兩個已排序的局部二進位檔案進行雙路歸併
        ifstream f1(file1, ios::binary); // 註解：以二進位模式開啟左側輸入二進位檔案
        ifstream f2(file2, ios::binary); // 註解：以二進位模式開啟右側輸入二進位檔案
        ofstream fOut(outFile, ios::binary); // 註解：以二進位模式開啟結果輸出檔案
        vector<Record> buffer1(100); // 註解：配置 100 筆 Record 之輸入快取一，限制內存開銷
        vector<Record> buffer2(100); // 註解：配置 100 筆 Record 之輸入快取二，限制內存開銷
        vector<Record> bufferOut(100); // 註解：配置 100 筆 Record 之輸出快取，以批次寫入降低磁碟存取次數
        int ptr1 = 0, count1 = 0; // 註解：快取一之當前讀取指針及實際載入量
        int ptr2 = 0, count2 = 0; // 註解：快取二之當前讀取指針及實際載入量
        int outCount = 0; // 註解：輸出快取之當前寫入計數器
        auto getRecord1 = [&]() -> Record* { // 註解：分批磁碟載入 Lambda 函式（適用於第一個檔案輸入流）
            if (ptr1 >= count1) { // 註解：當緩衝區資料被消耗完畢
                f1.read(reinterpret_cast<char*>(buffer1.data()), 100 * sizeof(Record)); // 註解：自磁碟載入至多 100 筆新資料
                count1 = f1.gcount() / sizeof(Record); // 註解：記錄本次成功獲取的實體資料長度
                ptr1 = 0; // 註解：將快取讀取指針歸零
            } // 註解：結束快取更新邏輯
            if (count1 == 0) return nullptr; // 註解：代表檔案已達 EOF，回傳空指針
            return &buffer1[ptr1]; // 註解：回傳快取中當前紀錄的指針
        }; // 註解：結束載入函數 Lambda
        auto getRecord2 = [&]() -> Record* { // 註解：分批磁碟載入 Lambda 函式（適用於第二個檔案輸入流）
            if (ptr2 >= count2) { // 註解：當緩衝區資料被消耗完畢
                f2.read(reinterpret_cast<char*>(buffer2.data()), 100 * sizeof(Record)); // 註解：自磁碟載入至多 100 筆新資料
                count2 = f2.gcount() / sizeof(Record); // 註解：記錄本次成功獲取的實體資料長度
                ptr2 = 0; // 註解：將快取讀取指針歸零
            } // 註解：結束快取更新邏輯
            if (count2 == 0) return nullptr; // 註解：代表檔案已達 EOF，回傳空指針
            return &buffer2[ptr2]; // 註解：回傳快取中當前紀錄的指針
        }; // 註解：結束載入函數 Lambda
        auto writeRecord = [&](const Record& rec) { // 註解：輸出流批次寫入快取之控制 Lambda
            bufferOut[outCount++] = rec; // 註解：將指定紀錄搬移至輸出快取並累加計數
            if (outCount == 100) { // 註解：當輸出快取已滿 100 筆時
                fOut.write(reinterpret_cast<const char*>(bufferOut.data()), 100 * sizeof(Record)); // 註解：進行一次性硬碟批次寫入
                outCount = 0; // 註解：重置輸出寫入指針計數器
            } // 註解：結束批次寫出邏輯
        }; // 註解：結束輸出快取 Lambda
        Record* r1 = getRecord1(); // 註解：讀取左側 Run 檔案之起始項目
        Record* r2 = getRecord2(); // 註解：讀取右側 Run 檔案之起始項目
        while (r1 != nullptr || r2 != nullptr) { // 註解：進行線性合併雙指針掃描
            if (r1 != nullptr && r2 != nullptr) { // 註解：當兩側皆具備可用資料時進行大小比較
                if (r1->weight > r2->weight) { // 註解：左側資料權重較大
                    writeRecord(*r1); // 註解：寫入左側紀錄到輸出緩衝區
                    ptr1++; // 註解：更新左側指針
                    r1 = getRecord1(); // 註解：載入下一筆左側紀錄
                } else if (r1->weight < r2->weight) { // 註解：右側資料權重較大
                    writeRecord(*r2); // 註解：寫入右側紀錄到輸出緩衝區
                    ptr2++; // 註解：更新右側指針
                    r2 = getRecord2(); // 註解：載入下一筆右側紀錄
                } else { // 註解：當兩側權重完全相等時，為保證 Stable 排序，必須優先寫入左側紀錄（即檔案中原本位置靠前的人）
                    writeRecord(*r1); // 註解：寫入左側紀錄
                    ptr1++; // 註解：更新左側指針
                    r1 = getRecord1(); // 註解：載入下一筆左側紀錄
                } // 註解：結束大小與穩定性比較
            } else if (r1 != nullptr) { // 註解：當右側檔案已耗盡，僅剩左側檔案時
                writeRecord(*r1); // 註解：依序寫出剩餘之左側紀錄
                ptr1++; // 註解：更新左側快取讀取指標
                r1 = getRecord1(); // 註解：獲取下一筆左側紀錄
            } else { // 註解：當左側檔案已耗盡，僅剩右側檔案時
                writeRecord(*r2); // 註解：依序寫出剩餘之右側紀錄
                ptr2++; // 註解：更新右側快取讀取指標
                r2 = getRecord2(); // 註解：獲取下一筆右側紀錄
            } // 註解：結束雙通道資料狀態分支處理
        } // 註解：結束線性合併迴圈
        if (outCount > 0) { // 註解：若歸併結束後，輸出緩衝區有少於 100 筆之殘留資料
            fOut.write(reinterpret_cast<const char*>(bufferOut.data()), outCount * sizeof(Record)); // 註解：沖刷緩衝區，寫入剩餘所有位元組
        } // 註解：結束輸出快取沖刷邏輯
        f1.close(); // 註解：釋放第一個輸入二進位檔案描述符
        f2.close(); // 註解：釋放第二個輸入二進位檔案描述符
        fOut.close(); // 註解：釋放寫入檔案描述符並完成寫入
    } // 註解：結束 mergeRuns 實體方法
public: // 註解：公開成員區段，向外部提供標準操作介面
    ExternalSorter(const string& num) : fileNum(num) {} // 註解：建構子，配置待處理的測試檔編號
    bool executeSort(double& tInternal, double& tExternal) { // 註解：外部排序控制器，管理初始 Run 分割及階梯式歸併
        auto tStart = chrono::high_resolution_clock::now(); // 註解：擷取高精度系統計時起點，標記內部排序起始
        string inName = "pairs" + fileNum + ".bin"; // 註解：組合對應目標二進位輸入檔名
        ifstream inFile(inName, ios::binary); // 註解：以 binary 模式建立檔案唯讀流
        if (!inFile.is_open()) return false; // 註解：檔案不存在時防呆直接中斷並回傳 false
        int runIndex = 0; // 註解：歸併第 0 階生成的 Runs 個數計數器
        vector<Record> memBuffer(300); // 註解：記憶體工作快取（大小固定為 300 筆 Record），完美滿足記憶體預算
        while (true) { // 註解：分批載入並在 RAM 中進行 initial runs 排序
            inFile.read(reinterpret_cast<char*>(memBuffer.data()), 300 * sizeof(Record)); // 註解：最大批次載入 300 筆資料
            int countRead = inFile.gcount() / sizeof(Record); // 註解：取得此批次之實體紀錄數量
            if (countRead == 0) break; // 註解：若檔案已完全載入完畢則結束迴圈
            vector<Record> activeBlock(memBuffer.begin(), memBuffer.begin() + countRead); // 註解：僅對實際讀到的區間進行向量初始化
            stable_sort(activeBlock.begin(), activeBlock.end(), [](const Record& a, const Record& b) { // 註解：進行穩定內部排序
                return a.weight > b.weight; // 註解：第一關鍵字 weight 由大到小降序排列
            }); // 註解：結束 Lambda 內建排序器
            string runName = getRunName(0, runIndex); // 註解：建構當前 Pass-0 的分段暫存檔名
            ofstream runFile(runName, ios::binary); // 註解：開啟暫存檔案寫入流
            runFile.write(reinterpret_cast<const char*>(activeBlock.data()), countRead * sizeof(Record)); // 註解：將排好序的 300 筆資料塊寫出到二進位檔
            runFile.close(); // 註解：關閉檔案以確保緩衝區寫入磁碟並釋放系統控制權
            runIndex++; // 註解：累加初始 Run 編號
        } // 註解：結束初始 Runs 劃分
        inFile.close(); // 註解：釋放原始檔案資源
        auto tInternalEnd = chrono::high_resolution_clock::now(); // 註解：取得內部排序完成之時間節點，作為外部歸併排序起點
        cout << "\nThe internal sort is completed. Check the initial sorted runs! \n\n" // 註解：輸出內部排序完成提示
             << "Now there are " << runIndex << " runs.\n\n"; // 註解：輸出首波 initial runs 總數
        int currentPassRuns = runIndex; // 註解：變數儲存當前階段必須處理的 Runs 檔案數量
        int pass = 0; // 註解：當前多路歸併遞迴深度深度
        while (currentPassRuns > 1) { // 註解：開始外部合併，重疊歸併至僅剩一個 Run 檔案為止
            int nextPassRuns = 0; // 註解：記錄新一輪合併後產生的新暫存檔個數
            for (int i = 0; i < currentPassRuns; i += 2) { // 註解：以步長為 2 線性合併相鄰檔案
                if (i + 1 < currentPassRuns) { // 註解：左右對稱存在，可以成對歸併
                    string r1 = getRunName(pass, i); // 註解：前一階段左側暫存檔
                    string r2 = getRunName(pass, i + 1); // 註解：前一階段右側暫存檔
                    string rOut = getRunName(pass + 1, nextPassRuns); // 註解：新生成的合併暫存檔
                    mergeRuns(r1, r2, rOut); // 註解：執行歸併核心函式
                    std::remove(r1.c_str()); // 註解：清理前一階段之左側暫存檔，確保硬碟空間不膨脹
                    std::remove(r2.c_str()); // 註解：清理前一階段之右側暫存檔，確保硬碟空間不膨脹
                    nextPassRuns++; // 註解：累加新階段產生的 Runs 數
                } else { // 註解：奇數個 Run 時剩餘的一個孤立暫存檔，直接遞補前進至下個階段
                    string r1 = getRunName(pass, i); // 註解：前一階段殘留的單一暫存檔
                    string rOut = getRunName(pass + 1, nextPassRuns); // 註解：移交至下一階段之對應暫存檔名
                    std::rename(r1.c_str(), rOut.c_str()); // 註解：重新命名暫存檔案位置
                    nextPassRuns++; // 註解：累加新階段產生的 Runs 數
                } // 註解：結束奇偶檔案流分支處理
            } // 註解：結束當前階層的所有 Run 對合併
            pass++; // 註解：更新合併階數深度
            currentPassRuns = nextPassRuns; // 註解：更新下一循環需處理的 Runs 數目
            cout << "Now there are " << currentPassRuns << " runs.\n\n"; // 註解：依照 DEMO 格式，印出當前剩餘 runs 的數量
        } // 註解：結束外部合併主邏輯
        string finalTempRun = getRunName(pass, 0); // 註解：取得歸併至最後唯一的頂層 Run 二進位檔
        string sortedOutName = "order" + fileNum + ".bin"; // 註解：建構題目要求之最終排序結果檔名
        std::remove(sortedOutName.c_str()); // 註解：主動清除先前可能殘留的舊 order*.bin 檔案以防寫入干擾
        std::rename(finalTempRun.c_str(), sortedOutName.c_str()); // 註解：移交命名為 order*.bin，結束外部排序
        auto tExternalEnd = chrono::high_resolution_clock::now(); // 註解：擷取外部合併與重命名結束的時間戳
        tInternal = chrono::duration<double, milli>(tInternalEnd - tStart).count(); // 註解：統計內部排序段所經歷的毫秒時間
        tExternal = chrono::duration<double, milli>(tExternalEnd - tInternalEnd).count(); // 註解：統計外部歸併段所經歷的毫秒時間
        return true; // 註解：成功處理完畢，回傳 true
    } // 註解：結束 executeSort 實體方法
    void buildPrimaryIndex() { // 註解：為已排序的 order*.bin 二進位檔案建立在記憶體中的主索引並印出
        string sortedOutName = "order" + fileNum + ".bin"; // 註解：建構目標已排序檔案的名稱
        ifstream f(sortedOutName, ios::binary); // 註解：建立目標已排序檔案之二進位唯讀流
        if (!f.is_open()) return; // 註解：防呆驗證，若目標檔案不存在則不作任何處理
        vector<IndexEntry> primaryIndex; // 註解：配置主索引暫存向量空間
        Record rec; // 註解：配置用以讀取單筆資料的 Record 結構體工作變數
        int recordIndex = 0; // 註解：追蹤當前掃描的 0-based 邏輯資料筆數序號
        float lastWeight = -1.0f; // 註解：標記前一次讀到的權重資訊，用於邊界感應
        while (f.read(reinterpret_cast<char*>(&rec), sizeof(Record))) { // 註解：批次向記憶體讀入單筆紀錄，極限節省 RAM 開銷
            if (primaryIndex.empty() || rec.weight != lastWeight) { // 註解：若為首筆或是資料權重發生改變（代表進入新權重區間）
                primaryIndex.push_back({rec.weight, recordIndex}); // 註解：紀錄該 unique 權重之起始邏輯行位置偏移
                lastWeight = rec.weight; // 註解：同步更新上次讀取的權重
            } // 註解：結束區段索引記錄邏輯
            recordIndex++; // 註解：累加資料偏移序號
        } // 註解：結束檔案遍歷
        f.close(); // 註解：關閉目標檔案流，釋放描述符控制權
        cout << "<Primary index>: (key, offset)\n"; // 註解：輸出任務二主索引格式裝飾行
        for (size_t i = 0; i < primaryIndex.size(); ++i) { // 註解：依序走訪所有主索引條目
            cout << "[" << i + 1 << "] (" << primaryIndex[i].weight << ", " << primaryIndex[i].recordOffset << ")\n"; // 註解：以 cout 預設精確度印出 (key, offset) 對應紀錄
        } // 註解：結束主索引格式印出 loop
    } // 註解：結束 buildPrimaryIndex 實體方法
}; // 註解：結束 ExternalSorter 類別定義
int main() { // 註解：程式進入點
    while (true) { // 註解：最外層主控制迴圈，允許使用者連續分析多個測試數據集
        cout << "* Data Structures and Algorithms *\n" // 註解：主選單第一列，顯示課程名稱
             << "**********************************\n" // 註解：顯示邊框裝飾線
             << "* 1. External merge sort on file *\n" // 註解：顯示功能一：外部排序
             << "* 2: Construct the primary index *\n" // 註解：顯示功能二：主索引建置
             << "**********************************\n" // 註解：顯示邊框裝飾線
             << "*** The buffer size is 300\n"; // 註解：顯示記憶體緩衝區配置大小資訊列
        string fileNum = ""; // 註解：宣告並初始化目標檔案名稱儲存變數
        while (true) { // 註解：檔案防呆驗證輸入小迴圈
            cout << "##################################\n" // 註解：Mission 1 提示裝飾列一
                 << "Mission 1: External merge sort \n" // 註解：Mission 1 標題
                 << "##################################\n\n" // 註解：Mission 1 提示裝飾列二及雙換行
                 << "Input the file name: [0]Quit\n"; // 註解：提示使用者輸入檔案編號或 0 退出
            if (!getline(cin, fileNum)) { // 註解：讀取一行輸入，若讀到 EOF
                fileNum = "0"; // 註解：強制將命令設為 0 以觸發正常結束邏輯
                break; // 註解：中斷輸入迴圈
            } // 註解：結束輸入偵測判斷
            if (!fileNum.empty() && fileNum.back() == '\r') { // 註解：過濾跨平台換行符影響
                fileNum.pop_back(); // 註解：剔除尾部無效的 \r
            } // 註解：結束換行符號過濾
            if (fileNum == "0") { // 註解：使用者輸入 0 表示直接進入結束控制
                break; // 註解：跳出輸入迴圈
            } // 註解：結束 0 判斷
            string testName = "pairs" + fileNum + ".bin"; // 註解：組裝完整的實體輸入二進位檔名
            ifstream testFile(testName, ios::binary); // 註解：嘗試唯讀方式開啟該檔案
            if (testFile.is_open()) { // 註解：若能成功開啟
                testFile.close(); // 註解：立即關閉該測試流，確認檔案合法存在
                break; // 註解：跳出防呆輸入迴圈，準備排序
            } else { // 註解：若無法開啟檔案，代表輸入無效或檔案缺失
                cout << "\npairs" << fileNum << ".bin does not exist!!!\n\n"; // 註解：輸出錯誤防呆警示，重新要求輸入
            } // 註解：結束測試開啟狀態分支
        } // 註解：結束輸入迴圈
        if (fileNum == "0") { // 註解：若被要求終止操作（檔名編號為 0）
            cout << "\n[0]Quit or [Any other key]continue?\n"; // 註解：進入結束詢問介面
            string cont = ""; // 註解：宣告詢問回答字串
            if (!getline(cin, cont)) { // 註解：若讀取回答時遭遇 EOF 則完全結束
                break; // 註解：跳出最外層主控制迴圈，程式結束
            } // 註解：結束回答讀取
            if (!cont.empty() && cont.back() == '\r') { // 註解：移移除 carriage return 符號
                cont.pop_back(); // 註解：剔除無效換行
            } // 註解：結束 carriage return 過濾
            if (cont == "0") { // 註解：若使用者回答 0，代表完全關閉程式
                break; // 註解：跳出外層大迴圈，徹底結束執行
            } // 註解：結束 0 確認退出
            cout << "\n"; // 註解：印出換行符，保持格式跟 DEMO 的空行完美契合
            continue; // 註解：回到外層大迴圈起始點，重新呈現主畫面
        } // 註解：結束檔名為 0 處置
        ExternalSorter sorter(fileNum); // 註解：實例化外部排序管理物件，將檔案流水號封裝進入 sorter 實體
        double tInternal = 0.0, tExternal = 0.0; // 註解：宣告儲存內部與外部排序耗時的浮點變數
        if (sorter.executeSort(tInternal, tExternal)) { // 註解：執行 sorter 的 executeSort 核心方法並驗證成功性
            double tTotal = tInternal + tExternal; // 註解：精確累加內部排序與外部歸併之總耗時
            stringstream ssInternal, ssExternal, ssTotal; // 註解：採用 stringstream 專屬流，避免 global cout 狀態被 persistent fixed 所干擾
            ssInternal << fixed << setprecision(3) << tInternal; // 註解：將內部排序時間限制於小數點後三位
            ssExternal << fixed << setprecision(3) << tExternal; // 註解：將外部合併時間限制於小數點後三位
            ssTotal << fixed << setprecision(3) << tTotal; // 註解：將總執行時間限制於小數點後三位
            cout << "The execution time ...\n" // 註解：輸出時間統計標題
                 << "Internal Sort = " << ssInternal.str() << " ms\n" // 註解：印出內部排序時間字串
                 << "External Sort = " << ssExternal.str() << " ms\n" // 註解：印出外部排序時間字串
                 << "Total Execution Time = " << ssTotal.str() << " ms\n\n" // 註解：印出總排序時間字串
                 << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n" // 註解：任務二裝飾界線一
                 << "Mission 2: Build the primary index \n" // 註解：任務二標題
                 << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n\n"; // 註解：任務二裝飾界線二及換行
            sorter.buildPrimaryIndex(); // 註解：執行 sorter 物件的 buildPrimaryIndex 實體方法，快速分析已排序檔案並輸出主索引
        } // 註解：結束排序執行與成果展示
        cout << "\n[0]Quit or [Any other key]continue?\n"; // 註解：在全套流程完畢後，提示使用者是否要完全退出或繼續分析下一檔案
        string action = ""; // 註解：宣告儲存使用者接續動作的字串變數
        if (!getline(cin, action)) { // 註解：讀取使用者輸入，若讀到 EOF
            break; // 註解：跳出最外層主控制迴圈，程式結束
        } // 註解：結束接續指令讀取
        if (!action.empty() && action.back() == '\r') { // 註解：移移除 carriage return 符號
            action.pop_back(); // 註解：剔除無效換行
        } // 註解：結束 carriage return 過濾
        if (action == "0") { // 註解：若使用者輸入 0 代表結束執行
            break; // 註解：跳出主程式控制迴圈，正常結束程式
        } // 註解：結束 0 退出分支判斷
        cout << "\n"; // 註解：印出一個換行以使畫面跟 DEMO 完全契合
    } // 註解：結束外層大迴圈
    return 0; // 註解：程式正常結束，回傳系統代碼 0
} // 註解：結束 main 進入點函數
