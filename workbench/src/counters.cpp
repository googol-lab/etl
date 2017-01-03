//=======================================================================
// Copyright (c) 2014-2016 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <iostream>
#include <chrono>
#include <random>

#define ETL_COUNTERS

#include "etl/etl.hpp"

typedef std::chrono::high_resolution_clock timer_clock;
typedef std::chrono::milliseconds milliseconds;

float fake = 0;

/*
 *
 * Current values are:
 * Simple: 30 / 20 / 10
 * Basic: 40 / 20 / 10
 */

void simple(){
    etl::dyn_matrix<float, 2> A(4096, 4096);
    etl::dyn_matrix<float, 2> B(4096, 4096);
    etl::dyn_matrix<float, 2> C(4096, 4096);

    A = etl::normal_generator<float>(1.0, 0.0);
    B = etl::normal_generator<float>(1.0, 0.0);
    C = etl::normal_generator<float>(1.0, 0.0);

    etl::reset_counters();

    std::cout << "Simple" << std::endl;

    for (size_t i = 0; i < 10; ++i) {
        C = A * B;
        fake += etl::sum(C);
    }

    etl::dump_counters();
}

void basic(){
    etl::dyn_matrix<float, 2> A(4096, 4096);
    etl::dyn_matrix<float, 2> B(4096, 4096);
    etl::dyn_matrix<float, 2> C(4096, 4096);
    etl::dyn_matrix<float, 2> D(4096, 4096);

    A = etl::normal_generator<float>(1.0, 0.0);
    B = etl::normal_generator<float>(1.0, 0.0);
    C = etl::normal_generator<float>(1.0, 0.0);
    D = etl::normal_generator<float>(1.0, 0.0);

    etl::reset_counters();

    std::cout << "Basic" << std::endl;

    for (size_t i = 0; i < 10; ++i) {
        C = A * B * B;
        D += C;
        D *= 1.1;
        fake += etl::mean(D);
    }
    etl::dump_counters();
}

int main(){
    auto start_time = timer_clock::now();

    simple();
    basic();

    auto end_time = timer_clock::now();
    auto duration = std::chrono::duration_cast<milliseconds>(end_time - start_time);

    std::cout << "duration: " << duration.count() << "ms" << std::endl;

    return (int) fake;
}