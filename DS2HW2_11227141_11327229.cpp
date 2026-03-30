//11227141 鍾博竣
//11327229 游啓揚

#include <iostream>
#include <string>  //字串
#include <sstream>  //檢查輸入
#include <fstream> //read file
#include <vector>
#include <iomanip> //控制輸出格式

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

struct Node {
    //二維陣列，第一維是放相同的值，第二層是放不同的值，最多三個相同的值，最多三個不同的值
    vector<vector<Record>> keys;
    Node* children[4];
    Node* parent;
    int keyCount;

    //initialize the node
    Node() : parent(nullptr), keyCount(0) {
      for (int i = 0; i < 4; ++i) {
        children[i] = nullptr;
      }
      keys.resize(3); //最多三個不同的值
    }
};

class TwoThreeTree {
 private:
  Node *root;

  bool isLeaf(const Node* &node) {
    // if the first child is null, then it's a leaf node
    return (node->children[0] == nullptr);
  }

  Node* findLeafNode(Record &record) {
    Node* current = root;

    //由於2-3樹往上生長，故不用檢查nullptr，他一定會有兩~三個節點存在
    while (!isLeaf(current)) {
      if (record.graduates < current->keys[0].graduates) {
        current = current->children[0];
      } else if (record.graduates == current->keys[0].graduates) {
        current->keys[0].push_back(record);
        current = nullptr; //直接存相同的值
      } else if (current->key_count == 1) {
        current = current->children[1];
      } else {
        if (record.graduates == current->keys[1].graduates) {
          current->keys[2].push_back(record);
          current = nullptr; //直接存相同的值
        }
        else if (record.graduates < current->keys[1].graduates) {
          current = current->children[1];
        } else {
          current = current->children[2];
        }
      }
    }
    return current;
  }

  void sortKeys(Node* node, Record &record) {
    if (record < node->keys[0]) {
      node->keys.push_back(node->keys[1]);
      node->keys[1] = node->keys[0];
      node->keys[0] = record;
    } else if (record < node->keys[1]) {
      node->keys.push_back(node->keys[1]);
      node->keys[1] = record;
    } else {
      node->keys.push_back(record);
    }
  }

  void splitNode(Node* node) {
    if (node->parent == nullptr) {
      Node* new_root = new Node();

      new_root->keys[0] = node->keys[1]; //將中間的值提升到父節點
      new_root->children[0] = node; //原本的節點成為新的 root 的第一個子節點

      new_root->children[1] = new Node(); //建立新的節點，成為新的 root 的第二個子節點
      new_root->children[1]->keys[0] = node->keys[2]; //將原本的第三個值移到新的節點

      new_root->key_count = node->key_count = new_root->children[1]->key_count = 1; //更新 key_count

      node->keys[1] = node->keys[2] = Record(); //清空原本的值

      node->parent = new_root->children[1]->parent = new_root; //更新父節點
      root = new_root; //更新 root
    } else if (node->parent->key_count == 1) {
      Node* new_node = new Node();
      Node* parent_node = node->parent;

      if (node == parent_node->children[0]) {
        parent_node->children[2] = parent_node->children[1]; //將原本的第二個子節點移到第三個子節點
        parent_node->children[1] = new_node; //將新的節點放在第二個子節點的位置
        new_node->parent = parent_node;

        new_node->keys[0] = node->keys[2]; // 將原本的第三個值移到新的節點
        new_node->key_count = 1;

        parent_node->keys.insert(parent_node->keys.begin(), node->keys[1]); //將原本的第二個值提升到父節點
        parent_node->key_count = 2;

        node->keys[1] = node->keys[2] = Record(); //清空原本的值
        node->key_count = 1;
      } else {
        parent_node->children[2] = new_node; //將新的節點放在第三個子節點的位置
        new_node->parent = parent_node;

        new_node->keys[0] = node->keys[2]; // 將原本的第三個值移到新的節點
        new_node->key_count = 1;

        parent_node->keys.push_back(node->keys[1]);
        parent_node->key_count = 2;

        node->keys[1] = node->keys[2] = Record(); //清空原本的值
        node->key_count = 1;
      }
    } else {
      //父節點已經有兩個值了，先排序後分裂
      sortKeys(node->parent, node->keys[1]);
      node->keys[1] = node->keys[2];
      node->keys[2] = Record();
      node->key_count = 2;
      splitNode(node->parent); //遞迴分裂父節點
    }

    return;
  }

  void insertOne(Record &record) {
    //建立 root 節點
    if (root == nullptr) {
      root = new Node();
      root->keys[0] = record;
      root->key_count = 1;
      return;
    }

    /*找到葉子層*/
    Node* probe = findLeafNode(record);

    if (probe == nullptr) {
      return; //已經插入相同的值了
    } else {
      if (probe->key_count == 1) {
        if (record.graduates < probe->keys[0].graduates) {
          probe->keys.push_back(probe->keys[0]);
          probe->keys[0] = record;
        } else {
          probe->keys.push_back(record);
        }
        probe->key_count = 2;
      } else {
        //節點內的值大於兩個，先排序後分裂
        sortKeys(probe, record);
        splitNode(probe);
      }
    }

    return;
  }
 public:
  TwoThreeTree() {
    root = nullptr;
  }

  void insert(vector<Record> records_list) {
    for (int i = 0; i < records_list.size(); ++i) {
     heap.insertOne(records_list[i]);
    }
    return;
  }

  void printTargetNodes() {
    //TODO：printTargetNodes
    return;
  }
};

bool loadFile(const string &filename, vector<Record> &records_list);
bool isInteger(const string &input);
bool processCommand(int cmd);

int main() {
  while (true) {
    cout << "* Data Structures and Algorithms *\n" 
    << "****** Balanced Search Tree ******\n"
    << "* 0. QUIT                        *\n" 
    << "* 1. Build 23 tree               *\n" 
    << "* 2. Build AVL tree              *\n" 
    << "**********************************\n" 
    << "Input a choice(0, 1, 2): ";

    string command;
    getline(cin, command);
    if (!command.empty() && command.back() == '\r') command.pop_back();
    
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
      if (line.empty()) continue;
      if (line.back() == '\r') line.pop_back();

      stringstream ss(line);
      string token;
      vector<string> tokens;
      // Split line by tabs
      while(getline(ss, token, '\t')) {
          tokens.push_back(token);
      }
        
      if (tokens.size() < 11_) {
        cotinue; // skip this line if it doesn't have enough fields
      }

      Record r;
      r.seqId = seqBase++;
      r.schoolId = tokens[0];
      r.schoolName = tokens[1];
      r.deptId = tokens[2];
      r.deptName = tokens[3];
      r.dayNight = tokens[4];
      r.level = tokens[5];
      r.students = stoi(tokens[6]);
      r.teachers = stoi(tokens[7]);
      r.graduates = stoi(tokens[8]);
      r.city = tokens[9];
      r.type = tokens[10];
        
      records_list.push_back(r);
    }
    file.close();
    return true;
}

bool isInteger(const string &input) {
  if (input.empty()) return false;

  stringstream temp(input);
  int number;
  char c;
  if (temp >> number && !(temp >> c)) {
    return true;
  }
  return false;
}

bool processCommand(int cmd) {
  string filename {};
  static vector<Record> records_list;

  if (cmd == 1) {
    TwoThreeTree tree;
    records_list = Record();

    do {
      cout << "\nInput a file number ([0] Quit): ";

      getline(cin, filename);
      if (filename.back() == '\r') filename.pop_back();
    
      if (filename == "0") {
        return true;
      }
    } while(!loadFile(filename, records_list));

    tree.insert(records_list);
    tree.printTargetNodes();
    return true;
  } else if (cmd == 2) {
    return true;
  }

  return false;
}