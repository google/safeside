#include <setjmp.h>
#include <signal.h>

#include <iostream>
#include <functional>

#include "timing_array.h"
#include "utils.h"

thread_local jmp_buf signal_handler_jmpbuf;

void SignalHandler(int signal, siginfo_t *info, void *ucontext) {
  std::cout << "got signal " << signal << std::endl;
  longjmp(signal_handler_jmpbuf, 1);
}

void Wrap(std::function<void()> inner) {
  struct sigaction sa, oldsa;
  sa.sa_sigaction = SignalHandler;
  sigaction(SIGSEGV, &sa, &oldsa);

  if (setjmp(signal_handler_jmpbuf) == 0) {
    inner();
    std::cerr << "returned from function that was supposed to fault"
              << std::endl;
    exit(1);
  }

  sigaction(SIGSEGV, &oldsa, nullptr);
}

int main(int argc, char* argv[]) {
  TimingArray ta;

  ta.FlushFromCache();
  Wrap([&ta]() {
    char* x = nullptr;
    *(volatile char *)x;

    ForceRead(&ta[5]);
  });

  std::cout << ta.FindFirstCachedElementIndex() << std::endl;

  std::cout << "normal return" << std::endl;
  return 0;
}
