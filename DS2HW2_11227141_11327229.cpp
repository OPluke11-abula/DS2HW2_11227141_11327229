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
  int id;
  int graduates;
  Record(int i = 0, int g = 0) : id(i), graduates(g) {}
};

class TwoThreeTree {
 private:
  
 public:
  TwoThreeTree() {
    heap.reserve(1000); //配置1000個Record空間
    heap.emplace_back(0, 0);
  }
};

bool loadFile(string filename);
bool isInteger(const string &input);
bool processCommand(int cmd);
bool getGraduates(const string &line, int &graduates);

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

bool loadFile(string filename) {
    filename = "input" + filename + ".txt";
    ifstream file(filename);

    if (!file.is_open()) {
        cout << "\n### " << filename << " does not exist! ###" << endl;
        return false;
    }

    int graduates = -1;
    string line;
    // 跳過標題列
    getline(file, line); 
    getline(file, line);
    getline(file, line);

    while (getline(file, line)) {
        if (line.empty()) continue;

        //TODO：
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

  if (cmd == 1 || cmd == 2) {
    //TODO：
    return true;
  }

  return false;
}