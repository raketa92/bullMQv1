#include "logger.hpp"
#include "thread_pool.hpp"
#include <iostream>

int main()
{
    ThreadPool pool(4);
    int a = 5, b = 10;
    pool.enqueue([ a, b ] {
        std::cout << "Task 1: " << a + b << '\n';
    });
}