#include <iostream>
#include <thread>
#include <chrono>

using namespace std;

int main() {
    
    int counter = 0;
    while (true)
    {
        /* code */
        cout << counter << " Hello, World!" << endl;
        counter++;
        if (counter > 5)        {
            break;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
    return 0;
}
