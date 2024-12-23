#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/file.h>
#include <thread>

class FileLock {
public:
  FileLock(const std::string &filename) {
    filePtr = fopen(filename.c_str(), "w+");
    if (!filePtr) {
      std::cerr << "Can't open file: " << filename << std::endl;
    }
  }

  ~FileLock() {
    if (filePtr) {
      fclose(filePtr);
    }
  }

  bool lock() {
    if (filePtr) {
      int fd = fileno(filePtr);
      if (flock(fd, LOCK_EX) == 0) {
        return true;
      } else {
        std::cerr << "Failed to lock file: Error: " << strerror(errno)
                  << std::endl;
        return false;
      }
    }
    return false;
  }

  bool isLocked() {
    if (filePtr) {
      int fd = fileno(filePtr);
      if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        flock(fd, LOCK_UN);
        return false;
      } else {
        return true;
      }
    }

    return false;
  }

  void unlock() {
    if (filePtr) {
      int fd = fileno(filePtr);
      flock(fd, LOCK_UN);
    }
  }

private:
  std::FILE *filePtr = nullptr;
};

int main() {
  FileLock fileLock("tmp/test.lock");

  if (fileLock.isLocked()) {
    std::cout << "File blocked by other process" << std::endl;
  }
  if (fileLock.lock()) {
    std::cout << "File blocked successfully" << std::endl;

    std::cout << "Waiting 10 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    fileLock.unlock();
    std::cout << "File unblocked" << std::endl;
  } else {
    std::cout << "Can't block file" << std::endl;
  }

  return 0;
}