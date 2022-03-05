#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <atomic>
#include <queue>

std::atomic_int threadCount = 0;
std::condition_variable cv;

std::mutex mutexForFiles;
std::mutex mutexForLines;

namespace fs = std::filesystem;

int globalCount = 0;

std::queue<std::filesystem::path> getPaths(std::filesystem::directory_iterator &p);
int getDirectory(int argc, char **argv, std::string &directory);
void getLineCount(std::queue<std::filesystem::path> &paths);
int getLineCountInFile(std::queue<std::filesystem::path> &paths, int &lineCount);
void addCount(int lineCount);
std::string GetFileContents(const fs::path& filePath);


int main(int argc, char **argv)
{
	try {
		if (std::string dir;  getDirectory(argc, argv, dir) == 0) {
			//auto p =  std::filesystem::recursive_directory_iterator(path);

			auto p = std::filesystem::directory_iterator(dir);

			auto paths = getPaths(p);

			const auto processor_count = std::thread::hardware_concurrency();


			std::vector<std::thread> threads;
			for (size_t i = 0; i < processor_count; i++) {
				threads.push_back(std::thread(getLineCount, std::ref(paths)));
			}


			std::unique_lock<std::mutex> lk(mutexForLines);
			while (threadCount != 0) {
				cv.wait(lk);
			}

			for (auto &tr : threads) {
				tr.join();
			}
		}
		else {
			std::cout << "enter directory name\n";
		}
		std::cout << globalCount;
	}
	catch (const fs::filesystem_error& err) {
		std::cerr << "filesystem error! " << err.what() << '\n';
	}
	catch (const std::runtime_error& err) {
		std::cerr << "runtime error! " << err.what() << '\n';
	}


	return 0;
}

std::string GetFileContents(const fs::path& filePath) {
	std::ifstream inFile{ filePath, std::ios::in | std::ios::binary };
	if (!inFile)
		throw std::runtime_error("Cannot open " + filePath.filename().string());

	std::string str(static_cast<size_t>(fs::file_size(filePath)), 0);

	inFile.read(str.data(), str.size());
	if (!inFile)
		throw std::runtime_error("Could not read the full contents from " + filePath.filename().string());

	return str;
}

void addCount(int lineCount)
{
	std::lock_guard<std::mutex> lg(mutexForLines);
	globalCount += lineCount;
}

int getLineCountInFile(std::queue<std::filesystem::path> &paths, int &lineCount)
{
	std::filesystem::path path;

	if (std::lock_guard<std::mutex> lg(mutexForFiles); !paths.empty()) {
		path = paths.front();
		paths.pop();
	}
	else {
		return -1;
	}

	auto text = GetFileContents(path);
	lineCount += std::count(std::begin(text), std::end(text), '\n');

	return 0;
}

void getLineCount(std::queue<std::filesystem::path> &paths)
{
	++threadCount;

	int lineCount = 0;
	while (true) {
		if (getLineCountInFile(paths, lineCount) == -1)
			break;
	}

	addCount(lineCount);
	--threadCount;
	cv.notify_one();
}

int getDirectory(int argc, char **argv, std::string &directory)
{
	if (argc > 1) {
		directory = argv[1];
	}
	else {
		return -1;
	}

	return 0;
}


std::queue<std::filesystem::path> getPaths(std::filesystem::directory_iterator &p)
{
	std::queue<std::filesystem::path> paths;
	for (auto &s : p) {
		if (s.is_regular_file())
			paths.push(s.path());
	}

	return paths;
}
