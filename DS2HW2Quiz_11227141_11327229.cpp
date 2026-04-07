// 11227141 鍾博竣
// 11327229 游啓揚

#include <algorithm> // for max
#include <fstream>   //read file
#include <iomanip>   //控制輸出格式
#include <iostream>
#include <sstream> //檢查輸入
#include <string>  //字串
#include <vector>

using namespace std;

struct Record {
  int seqId;
  string schoolId;
  string schoolName;
  string deptId;
  string deptName;
  string dayNight;
  string level;
  int students;
  int teachers;
  int graduates;
  string city;
  string type;

  Record() : seqId(0), students(0), teachers(0), graduates(0) {}
};

int compareDeptName(const string &lhs, const string &rhs) {
  if (lhs < rhs)
    return -1;
  if (lhs > rhs)
    return 1;
  return 0;
}

struct Node {
  // 二維陣列，第一維是放相同的值，第二層是放不同的值，最多三個相同的值，最多三個不同的值
  vector<vector<Record>> keys;
  Node *children[4];
  Node *parent;
  int key_count;

  // initialize the node
  Node() : parent(nullptr), key_count(0) {
    for (int i = 0; i < 4; ++i) {
      children[i] = nullptr;
    }
    keys.resize(3); // 最多三個不同的值
  }
};

class TwoThreeTree {
private:
  Node *root;

  int getHeight(Node *node) {
    if (node == nullptr)
      return 0;
    return 1 + getHeight(node->children[0]);
  }

  int countNodes(Node *node) {
    if (node == nullptr)
      return 0;
    int count = 1;
    for (int i = 0; i <= node->key_count; ++i) {
      count += countNodes(node->children[i]);
    }
    return count;
  }

  bool isLeaf(Node *node) {
    // if the first child is null, then it's a leaf node
    return (node->children[0] == nullptr);
  }

  Node *findLeafNode(Record &record) {
    Node *current = root;

    // 由於2-3樹往上生長，故不用檢查nullptr，他一定會有兩~三個節點存在
    while (!isLeaf(current)) {
      if (record.students < current->keys[0][0].students) {
        current = current->children[0];
      } else if (record.students == current->keys[0][0].students) {
        current->keys[0].push_back(record);
        return nullptr; // 直接存相同的值
      } else if (current->key_count == 1) {
        current = current->children[1];
      } else {
        if (record.students == current->keys[1][0].students) {
          current->keys[1].push_back(record);
          return nullptr; // 直接存相同的值
        } else if (record.students < current->keys[1][0].students) {
          current = current->children[1];
        } else {
          current = current->children[2];
        }
      }
    }

    // 抵達葉節點時也要檢查是否已經存在相同的值
    if (record.students == current->keys[0][0].students) {
      current->keys[0].push_back(record);
      return nullptr;
    } else if (current->key_count == 2 &&
               record.students == current->keys[1][0].students) {
      current->keys[1].push_back(record);
      return nullptr;
    }

    return current;
  }

  void sortKeys(Node *node, Record &record) {
    if (record.students < node->keys[0][0].students) {
      node->keys[2] = node->keys[1];
      node->keys[1] = node->keys[0];
      node->keys[0].clear();
      node->keys[0].push_back(record);
    } else if (record.students < node->keys[1][0].students) {
      node->keys[2] = node->keys[1];
      node->keys[1].clear();
      node->keys[1].push_back(record);
    } else {
      node->keys[2].clear();
      node->keys[2].push_back(record);
    }
  }

  void splitNode(Node *node) {
    // 1. 新建右側分裂出來的節點
    Node *new_node = new Node();
    new_node->keys[0] = node->keys[2];
    new_node->key_count = 1;

    // 將 node 的第 3 和第 4 個小孩過繼給 new_node (內部節點分裂)
    new_node->children[0] = node->children[2];
    new_node->children[1] = node->children[3];
    if (new_node->children[0] != nullptr)
      new_node->children[0]->parent = new_node;
    if (new_node->children[1] != nullptr)
      new_node->children[1]->parent = new_node;

    // 將準備提升的鍵暫存起來
    vector<Record> promote_key = node->keys[1];

    // 清空 node 右半部的值跟小孩
    node->keys[1].clear();
    node->keys[2].clear();
    node->children[2] = nullptr;
    node->children[3] = nullptr;
    node->key_count = 1;

    // 2. 處理父節點
    if (node->parent == nullptr) {
      // 升級成全新的 Root
      Node *new_root = new Node();
      new_root->keys[0] = promote_key;
      new_root->key_count = 1;

      new_root->children[0] = node;
      new_root->children[1] = new_node;

      node->parent = new_root;
      new_node->parent = new_root;
      root = new_root;
    } else {
      Node *parent_node = node->parent;

      // 尋找此 node 是父節點的第幾個小孩
      int child_idx = 0;
      while (child_idx <= parent_node->key_count &&
             parent_node->children[child_idx] != node) {
        child_idx++;
      }

      // 將父節點的 key 和小孩往右移動，騰出空間給新晉升的 key 和 new_node
      for (int i = parent_node->key_count; i > child_idx; i--) {
        parent_node->keys[i] = parent_node->keys[i - 1];
        parent_node->children[i + 1] = parent_node->children[i];
      }

      // 將值與新節點插入父節點
      parent_node->keys[child_idx] = promote_key;
      parent_node->children[child_idx + 1] = new_node;
      new_node->parent = parent_node;
      parent_node->key_count++;

      // 若父節點也滿了（達到 3 個 key），則連鎖觸發分裂
      if (parent_node->key_count == 3) {
        splitNode(parent_node);
      }
    }
  }

  void insertOne(Record &record) {
    // 建立 root 節點
    if (root == nullptr) {
      root = new Node();
      root->keys[0].push_back(record);
      root->key_count = 1;
      return;
    }

    /*找到葉子層*/
    Node *probe = findLeafNode(record);

    if (probe == nullptr) {
      return; // 已經插入相同的值了
    } else {
      if (probe->key_count == 1) {
        if (record.students < probe->keys[0][0].students) {
          probe->keys[1] = probe->keys[0];
          probe->keys[0].clear();
          probe->keys[0].push_back(record);
        } else {
          probe->keys[1].clear();
          probe->keys[1].push_back(record);
        }
        probe->key_count = 2;
      } else {
        // 節點內的值大於兩個，先排序後分裂
        sortKeys(probe, record);
        splitNode(probe);
      }
    }

    return;
  }

  // 遞迴刪除樹的所有節點，供解構子呼叫
  void clearNode(Node *node) {
    if (node == nullptr) return;
    for (int i = 0; i <= node->key_count; i++) {
      clearNode(node->children[i]);
    }
    delete node;
  }

  // 反向中序遍歷 2-3 樹，依【學生數】遞減順序收集各鍵值群組
  // 2-3 樹的中序為遞增，反向遍歷即得遞減
  void reverseInOrder(Node *node, vector<vector<Record>> &result) {
    if (node == nullptr) return;
    if (node->key_count == 2) {
      // 3-node: 右子樹 → keys[1] → 中子樹 → keys[0] → 左子樹
      reverseInOrder(node->children[2], result);
      result.push_back(node->keys[1]);
      reverseInOrder(node->children[1], result);
      result.push_back(node->keys[0]);
      reverseInOrder(node->children[0], result);
    } else {
      // 2-node: 右子樹 → keys[0] → 左子樹
      reverseInOrder(node->children[1], result);
      result.push_back(node->keys[0]);
      reverseInOrder(node->children[0], result);
    }
  }

public:
  TwoThreeTree() { root = nullptr; }

  void insert(vector<Record> records_list) {
    for (size_t i = 0; i < records_list.size(); ++i) {
      insertOne(records_list[i]);
    }
    return;
  }

  // 解構子：遞迴釋放所有節點的記憶體
  ~TwoThreeTree() { clearNode(root); }

  // 任務三：輸出【學生數】前 K 名最大值的紀錄
  // 若同值超過 K 筆，同值的額外紀錄都包含；同值依【序號】遞增排序
  void printTopK(int K) {
    if (root == nullptr) return;
    // 反向中序遍歷，取得各群組（學生數相同者為一群），已依遞減排列
    vector<vector<Record>> groups;
    reverseInOrder(root, groups);

    // 逐一加入群組，直到累計筆數 >= K（同值群組必須完整加入）
    vector<Record> output;
    int count = 0;
    for (auto &group : groups) {
      if (count >= K) break;
      // 將同值記錄依【序號】遞增排序
      vector<Record> sg = group;
      sort(sg.begin(), sg.end(),
           [](const Record &a, const Record &b) { return a.seqId < b.seqId; });
      for (auto &r : sg) output.push_back(r);
      count += (int)sg.size();
    }
    // 輸出結果
    int seq = 1;
    for (const auto &r : output) {
      cout << seq++ << ": [" << r.seqId << "] " << r.schoolName << ", "
           << r.deptName << ", " << r.dayNight << ", " << r.level << ", "
           << r.students << ", " << r.graduates << "\n";
    }
    cout << endl;
  }

  void printTargetNodes() {
    if (root == nullptr)
      return;
    int height = getHeight(root);
    int nodes = countNodes(root);
    cout << "Tree height = " << height << "\n";
    cout << "Number of nodes = " << nodes << "\n";

    int count = 1;
    for (int i = 0; i < root->key_count; ++i) {
      for (const auto &r : root->keys[i]) {
        cout << count++ << ": [" << r.seqId << "] " << r.schoolName << ", "
             << r.deptName << ", " << r.dayNight << ", " << r.level << ", "
             << r.students << ", " << r.graduates << "\n";
      }
    }
    cout << endl;
    return;
  }
};

// AVL 樹的節點
struct AVLNode {
  string deptName;        // 節點的鍵值（科系名稱）
  vector<Record> records; // 儲存相同科系名稱的所有資料
  AVLNode *left;
  AVLNode *right;
  int height; // 節點的高度，用於計算平衡因子

  // 建立新節點時，立刻將第一筆資料放入 records 中
  AVLNode(string name, Record rec)
      : deptName(name), left(nullptr), right(nullptr), height(1) {
    records.push_back(rec);
  }
};

class AVLTree {
private:
  AVLNode *root;

  // 遞迴刪除節點
  void clearNode(AVLNode *node) {
    if (node == nullptr)
      return;
    clearNode(node->left);
    clearNode(node->right);
    delete node;
  }

  // 取得節點高度
  int getHeight(AVLNode *node) {
    if (node == nullptr)
      return 0;
    return node->height;
  }

  // 計算平衡因子 (Left Height - Right Height)
  int getBalance(AVLNode *node) {
    if (node == nullptr)
      return 0;
    return getHeight(node->left) - getHeight(node->right);
  }

  // 右旋轉 (Right Rotate)
  // 處理 Left-Left 情況
  AVLNode *rightRotate(AVLNode *y) {
    AVLNode *x = y->left;
    AVLNode *T2 = x->right;

    // 執行旋轉
    x->right = y;
    y->left = T2;

    // 更新高度
    y->height = max(getHeight(y->left), getHeight(y->right)) + 1;
    x->height = max(getHeight(x->left), getHeight(x->right)) + 1;

    return x; // 回傳新的子樹根節點
  }

  // 左旋轉 (Left Rotate)
  // 處理 Right-Right 情況
  AVLNode *leftRotate(AVLNode *x) {
    AVLNode *y = x->right;
    AVLNode *T2 = y->left;

    // 執行旋轉
    y->left = x;
    x->right = T2;

    // 更新高度
    x->height = max(getHeight(x->left), getHeight(x->right)) + 1;
    y->height = max(getHeight(y->left), getHeight(y->right)) + 1;

    return y; // 回傳新的子樹根節點
  }

  // 將新紀錄插入 AVL 樹 (遞迴)
  AVLNode *insertNode(AVLNode *node, Record &record) {
    // 1. 一般的二元搜尋樹插入
    if (node == nullptr) {
      return new AVLNode(record.deptName, record);
    }

    int cmp = compareDeptName(record.deptName, node->deptName);

    if (cmp < 0) {
      node->left = insertNode(node->left, record);
    } else if (cmp > 0) {
      node->right = insertNode(node->right, record);
    } else {
      // 找到相同的科系名稱，將資料附加到同一個節點上，不增加新節點
      node->records.push_back(record);
      return node; // 樹的高度與結構未改變，直接回傳
    }

    // 2. 更新當前節點的高度
    node->height = 1 + max(getHeight(node->left), getHeight(node->right));

    // 3. 取得當前節點的平衡因子，檢查是否失衡
    int balance = getBalance(node);

    // 4. 處理 4 種失衡的情況

    // Left Left Case: 插入在左子節點的左側 -> 單次右旋轉
    // 以子樹的平衡因子 >= 0 判斷（左重或等高），比字串比較更穩健
    if (balance > 1 && getBalance(node->left) >= 0) {
      return rightRotate(node);
    }

    // Right Right Case: 插入在右子節點的右側 -> 單次左旋轉
    // 以子樹的平衡因子 <= 0 判斷（右重或等高）
    if (balance < -1 && getBalance(node->right) <= 0) {
      return leftRotate(node);
    }

    // Left Right Case: 插入在左子節點的右側 -> 先左旋轉再右旋轉
    // 左子樹的平衡因子 < 0 表示右側較重
    if (balance > 1 && getBalance(node->left) < 0) {
      node->left = leftRotate(node->left);
      return rightRotate(node);
    }

    // Right Left Case: 插入在右子節點的左側 -> 先右旋轉再左旋轉
    // 右子樹的平衡因子 > 0 表示左側較重
    if (balance < -1 && getBalance(node->right) > 0) {
      node->right = rightRotate(node->right);
      return leftRotate(node);
    }

    return node; // 回傳尚未改變的節點指標
  }

  // 計算樹的總節點數 (遞迴)
  int countNodes(AVLNode *node) {
    if (node == nullptr)
      return 0;
    return 1 + countNodes(node->left) + countNodes(node->right);
  }

  // 在 AVL 樹中精確搜尋科系名稱，回傳節點指標；找不到回傳 nullptr
  AVLNode *searchNode(AVLNode *node, const string &deptName) {
    if (node == nullptr) return nullptr;
    int cmp = compareDeptName(deptName, node->deptName);
    if (cmp == 0) return node;
    if (cmp < 0) return searchNode(node->left, deptName);
    return searchNode(node->right, deptName);
  }

public:
  AVLTree() { root = nullptr; }

  // 解構子: 在物件銷毀時釋放整棵樹的記憶體
  ~AVLTree() { clearNode(root); }

  // 插入一整包的資料
  void insert(vector<Record> &records_list) {
    for (size_t i = 0; i < records_list.size(); ++i) {
      root = insertNode(root, records_list[i]);
    }
    return;
  }

  void printTargetNodes() {
    if (root == nullptr)
      return;
    int height = getHeight(root);
    int nodes = countNodes(root);
    cout << "Tree height = " << height << "\n";
    cout << "Number of nodes = " << nodes << "\n";

    int count = 1;
    for (const auto &r : root->records) {
      cout << count++ << ": [" << r.seqId << "] " << r.schoolName << ", "
           << r.deptName << ", " << r.dayNight << ", " << r.level << ", "
           << r.students << ", " << r.graduates << "\n";
    }
    cout << endl;
    return;
  }

  // 公開查詢介面：精確搜尋科系名稱，回傳節點指標；找不到回傳 nullptr
  AVLNode *search(const string &deptName) {
    return searchNode(root, deptName);
  }

  // 任務四：精確搜尋【科系名稱】後，輸出【學生數】前 K 名最大值的紀錄
  // 若同值超過 K 筆，同值的額外紀錄都包含；同值依【序號】遞增排序
  void searchAndPrintTopK(const string &deptName, int K) {
    AVLNode *node = searchNode(root, deptName);
    if (node == nullptr) {
      cout << deptName << " is not found!\n";
      return;
    }
    // 依【學生數】遞減、【序號】遞增排序
    vector<Record> recs = node->records;
    sort(recs.begin(), recs.end(), [](const Record &a, const Record &b) {
      if (a.students != b.students) return a.students > b.students;
      return a.seqId < b.seqId;
    });
    // 逐一加入同值群組，直到累計 >= K（同值群組必須完整加入）
    vector<Record> output;
    int count = 0;
    size_t i = 0;
    while (i < recs.size()) {
      if (count >= K) break;
      int cur = recs[i].students;
      while (i < recs.size() && recs[i].students == cur) {
        output.push_back(recs[i++]);
        count++;
      }
    }
    // 輸出結果
    int seq = 1;
    for (const auto &r : output) {
      cout << seq++ << ": [" << r.seqId << "] " << r.schoolName << ", "
           << r.deptName << ", " << r.dayNight << ", " << r.level << ", "
           << r.students << ", " << r.graduates << "\n";
    }
    cout << endl;
  }
};

bool loadFile(const string &filename, vector<Record> &records_list);
bool isInteger(const string &input);
int parseInteger(string str);
bool processCommand(int cmd);

int main() {
  while (true) {
    cout << "* Data Structures and Algorithms *\n"
         << "****** Balanced Search Tree ******\n"
         << "* 0. QUIT                        *\n"
         << "* 1. Build 23 tree               *\n"
         << "* 2. Build AVL tree              *\n"
         << "* 3. Top-K max search on 23 tree *\n"
         << "* 4. Exact search on AVL tree    *\n"
         << "**********************************\n"
         << "Input a choice(0, 1, 2, 3, 4): ";

    string command;
    getline(cin, command);
    if (!command.empty() && command.back() == '\r')
      command.pop_back();

    if (isInteger(command)) {
      int cmd = stoi(command);
      if (cmd == 0) {
        return 0;
      } else if (!processCommand(cmd)) {
        cout << "\nCommand does not exist!" << endl;
      }
    } else {
      cout << "\nCommand does not exist!" << endl;
    }
    cout << '\n';
  }

  return 0;
}

bool loadFile(const string &filename, vector<Record> &records_list) {
  if (filename.empty()) {
    cout << "\n### " << "input" + filename + ".txt" << " does not exist! ###\n";
    return false;
  }
  string fullPath = "input" + filename + ".txt";
  ifstream file(fullPath);

  if (!file.is_open()) {
    cout << "\n### " << fullPath << " does not exist! ###\n";
    return false;
  }

  string line;
  // Skip 3 header lines
  getline(file, line);
  getline(file, line);
  getline(file, line);

  int seqBase = 1;
  // Read each line and parse the fields
  while (getline(file, line)) {
    if (line.empty())
      continue;
    if (line.back() == '\r')
      line.pop_back();

    stringstream ss(line);
    string token;
    vector<string> rawTokens;
    // Split line by tabs
    while (getline(ss, token, '\t')) {
      rawTokens.push_back(token);
    }

    // 處理 Excel 帶引號的數字欄位，例如 "1,047"
    // 這類欄位含有逗號，以 tab 分割後會被拆成兩個 token（如 ["1] 和 [047"]）
    // 偵測到開頭引號時，持續合併後續 token 直到找到結尾引號，再去除引號
    vector<string> tokens;
    for (int i = 0; i < (int)rawTokens.size(); i++) {
      string &t = rawTokens[i];
      if (!t.empty() && t.front() == '"') {
        // 開頭有引號，開始合併直到結尾引號出現
        string merged = t;
        while (merged.back() != '"' && i + 1 < (int)rawTokens.size()) {
          i++;
          merged += "," + rawTokens[i];
        }
        // Strip the surrounding quotes
        if (merged.size() >= 2 && merged.front() == '"' && merged.back() == '"')
          merged = merged.substr(1, merged.size() - 2);
        tokens.push_back(merged);
      } else {
        tokens.push_back(t);
      }
    }

    if (tokens.size() < 11) {
      continue; // skip this line if it doesn't have enough fields
    }

    Record r;
    r.seqId = seqBase++;
    r.schoolId = tokens[0];
    r.schoolName = tokens[1];
    r.deptId = tokens[2];
    r.deptName = tokens[3];
    r.dayNight = tokens[4];
    r.level = tokens[5];
    r.students = parseInteger(tokens[6]);
    r.teachers = parseInteger(tokens[7]);
    r.graduates = parseInteger(tokens[8]);
    r.city = tokens[9];
    r.type = tokens[10];

    records_list.push_back(r);
  }
  file.close();
  return true;
}

bool isInteger(const string &input) {
  if (input.empty())
    return false;

  stringstream temp(input);
  int number;
  char c;
  if (temp >> number && !(temp >> c)) {
    return true;
  }
  return false;
}

// 將字串安全轉換為整數，處理以下特殊格式：
//   - 千分位逗號：如 "1,047" -> 1047
//   - Excel 引號：如 "1047" -> 1047
//   - 空字串或僅有 "-"（表示無資料）-> 回傳 0
//   - 其他無法轉換的情況 -> 回傳 0
int parseInteger(string str) {
  str.erase(remove(str.begin(), str.end(), ','), str.end()); // 去除千分位逗號
  str.erase(remove(str.begin(), str.end(), '"'), str.end()); // 去除引號
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == string::npos)
    return 0; // 全為空白
  str = str.substr(first);
  if (str == "-")
    return 0; // 連字號表示無資料
  try {
    return stoi(str);
  } catch (...) {
    return 0; // 轉換失敗一律回傳 0
  }
}

bool processCommand(int cmd) {
  string filename{};
  static vector<Record>
      records_list; // 靜態變數，儲存從檔案讀取的資料，跨指令保留
  static bool isAVLTreeBuilt = false;           // 靜態紀錄 AVL Tree 是否已建立
  static AVLTree *avl_tree = nullptr;           // 持久化儲存 AVL Tree 實例
  static bool is23TreeBuilt = false;            // 靜態紀錄 2-3 樹是否已建立
  static TwoThreeTree *two_three_tree = nullptr; // 持久化儲存 2-3 樹實例

  if (cmd == 1) {
    records_list.clear();   // 重新選擇 1 時，清空舊有資料
    isAVLTreeBuilt = false; // 資料即將更新，重設 AVL Tree 建立狀態
    is23TreeBuilt = false;  // 重設 2-3 樹建立狀態
    if (avl_tree != nullptr) { // 如有舊的 AVL 樹則安全釋放記憶體
      delete avl_tree;
      avl_tree = nullptr;
    }
    if (two_three_tree != nullptr) { // 如有舊的 2-3 樹則安全釋放記憶體
      delete two_three_tree;
      two_three_tree = nullptr;
    }

    do {
      cout << "\nInput a file number ([0] Quit): ";
      getline(cin, filename);
      if (!filename.empty() && filename.back() == '\r') // 防止 \r 造成路徑錯誤
        filename.pop_back();
      if (filename == "0") return true;
    } while (!loadFile(filename, records_list));

    // 建立 2-3 樹並輸出結果
    two_three_tree = new TwoThreeTree();
    two_three_tree->insert(records_list);
    two_three_tree->printTargetNodes();
    is23TreeBuilt = true;
    return true;

  } else if (cmd == 2) {
    // 確認任務一已執行
    if (records_list.empty()) {
      cout << "### Choose 1 first. ###\n";
      return true;
    }
    // 若尚未建立過 AVL 樹，則建立並輸出
    if (!isAVLTreeBuilt) {
      avl_tree = new AVLTree();
      avl_tree->insert(records_list);
      avl_tree->printTargetNodes();
      isAVLTreeBuilt = true;
    } else {
      cout << "### AVL tree has been built. ###\n";
      avl_tree->printTargetNodes();
    }
    return true;

  } else if (cmd == 3) {
    // 任務三：2-3 樹 top-K 最大值搜尋
    if (!is23TreeBuilt || two_three_tree == nullptr) {
      cout << "### Choose 1 first. ###\n";
      return true;
    }
    // 提示合理範圍，N 為資料總筆數
    int N = (int)records_list.size();
    cout << "\nEnter K in [1," << N << "]: ";
    string kStr;
    getline(cin, kStr);
    if (!kStr.empty() && kStr.back() == '\r') kStr.pop_back();
    // K 必須在合理範圍 [1,N] 內；超出範圍或非整數則靜默返回
    if (!isInteger(kStr) || stoi(kStr) <= 0 || stoi(kStr) > N) return true;
    two_three_tree->printTopK(stoi(kStr));
    return true;

  } else if (cmd == 4) {
    // 任務四：AVL 樹字串精確搜尋
    // 先確認任務一已執行
    if (!is23TreeBuilt) {
      cout << "### Choose 1 first. ###\n";
      return true;
    }
    // 再確認任務二已執行
    if (!isAVLTreeBuilt || avl_tree == nullptr) {
      cout << "### Choose 2 first. ###\n";
      return true;
    }
    // 輸入科系名稱
    cout << "\nEnter a department name to search: ";
    string deptName;
    getline(cin, deptName);
    if (!deptName.empty() && deptName.back() == '\r') deptName.pop_back();

    // 精確搜尋
    AVLNode *node = avl_tree->search(deptName);
    if (node == nullptr) {
      cout << deptName << " is not found!\n";
      return true;
    }
    // 找到：提示 K 的合理範圍，M 為該科系的記錄總數
    int M = (int)node->records.size();
    cout << "Enter K in [1," << M << "]: ";
    string kStr;
    getline(cin, kStr);
    if (!kStr.empty() && kStr.back() == '\r') kStr.pop_back();
    // K 必須在合理範圍 [1,M] 內；超出範圍或非整數則靜默返回
    if (!isInteger(kStr) || stoi(kStr) <= 0 || stoi(kStr) > M) return true;
    avl_tree->searchAndPrintTopK(deptName, stoi(kStr));
    return true;
  }

  return false;
}
