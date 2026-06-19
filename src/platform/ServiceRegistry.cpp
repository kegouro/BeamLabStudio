#include "platform/ServiceRegistry.h"

#ifndef NDEBUG
namespace beamlab::platform {
thread_local std::vector<std::type_index> ServiceRegistry::resolutionStack_;
}
#endif

// ===================================================================
// Test (compile & run: g++ -std=c++17 -Isrc src/platform/ServiceRegistry.cpp -o /tmp/test_sr && /tmp/test_sr)
// ===================================================================
//
// #include <cassert>
// #include <iostream>
//
// using namespace beamlab::platform;
//
// struct Logger {
//     void log(const std::string& msg) { std::cout << "[log] " << msg << "\n"; }
// };
//
// struct Greeter {
//     explicit Greeter(Logger& logger) : logger_(logger) {}
//     void greet(const std::string& name) {
//         logger_.log("Hello, " + name + "!");
//     }
//   private:
//     Logger& logger_;
// };
//
// int main() {
//     ServiceRegistry reg;
//
//     // Register a singleton.
//     reg.registerSingleton<Logger>(std::make_unique<Logger>());
//
//     // Register auto-wired: Greeter needs Logger.
//     reg.registerSingleton<Greeter, Logger>();
//
//     // Resolve and use.
//     auto& greeter = reg.resolve<Greeter>();
//     greeter.greet("World");
//
//     // Scoped override.
//     {
//         auto scope = reg.createScope();
//         scope.registerSingleton<Logger>(std::make_unique<Logger>());
//         // ^ scope's logger is used instead of parent's
//     }
//     // ^ after scope exits, parent's Logger is restored
//
//     // Circular detection test (debug builds only):
//     // struct A { explicit A(B&) {} };
//     // struct B { explicit B(A&) {} };
//     // reg.registerSingleton<A, B>();
//     // reg.registerSingleton<B, A>();
//     // try {
//     //     reg.resolve<A>();  // should throw
//     // } catch (const std::runtime_error& e) {
//     //     std::cout << "Circular detected: " << e.what() << "\n";
//     // }
//
//     std::cout << "All OK\n";
//     return 0;
// }
