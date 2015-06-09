#include <iostream>
#include <list>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>


struct MemoryBlock {
    MemoryBlock(unsigned int begin, unsigned int end, int heapIndex) :
            begin(begin), end(end), heapIndex(heapIndex) {
    }

    bool isFree() const;

    unsigned int size() const {
        return end - begin;
    }

    unsigned int begin;
    unsigned int end;
    int heapIndex;
};

typedef std::list<MemoryBlock>::iterator MemoryBlockIterator;

class MemoryBlockIteratorShorter {
public:
    bool operator() (const MemoryBlockIterator& first,
            const MemoryBlockIterator& second) const {
        return (first->size() < second->size() ||
                (first->size() == second->size() && first->begin > second->begin));
    }
};


template <typename Iterator>
void setHeapIndex(const Iterator& iterator, int heapIndex) {
    iterator->heapIndex = heapIndex;
}


template <typename T, typename Comparator = std::less<T>>
class Heap {
public:
    Heap() : Heap(Comparator(), std::function<void(const T&, int)>()) {
    }
    explicit Heap(const Comparator& comparator) :
            Heap(comparator, std::function<void(const T&, int)>()) {
    }
    explicit Heap(const std::function<void(const T&, int)>& onIndexUpdate) :
            Heap(Comparator(), onIndexUpdate) {
    }
    Heap(const Comparator& comparator, const std::function<void(const T&, int)>& onIndexUpdate) :
            comparator(comparator), onIndexUpdate(onIndexUpdate) {
    }

    void insert(const T& element) {
        data.push_back(element);
        if (onIndexUpdate) {
            onIndexUpdate(data[data.size() - 1], data.size() - 1);
        }
        siftUp(data.size() - 1);
    }

    void pop() {
        remove(ROOT_INDEX);
    }

    const T& getMax() const {
        return data[ROOT_INDEX];
    }

    void remove(int index) {
        swap(index, data.size() - 1);
        if (onIndexUpdate) {
            onIndexUpdate(data[data.size() - 1], NO_HEAP_INDEX);
        }
        data.pop_back();
        siftUp(index);
        siftDown(index);
    }

    bool empty() const {
        return data.empty();
    }

    static const int NO_HEAP_INDEX = -1;

private:
    void siftUp(int index) {
        while (parent(index) != NO_HEAP_INDEX && comparator(data[parent(index)], data[index])) {
            swap(parent(index), index);
            index = parent(index);
        }
    }

    int largestSonIndex(int index) {
        int largest = left(index);
        if (largest != NO_HEAP_INDEX
                && right(index) != NO_HEAP_INDEX
                && comparator(data[largest], data[right(index)])) {
            largest = right(index);
        }
        return largest;
    }

    void siftDown(int index) {
        int largest = largestSonIndex(index);
        if (largest != NO_HEAP_INDEX
                && comparator(data[index], data[largest])) {
            swap(index, largest);
            siftDown(largest);
        }
    }

    void swap(int firstIndex, int secondIndex) {
        std::swap(data[firstIndex], data[secondIndex]);
        if (onIndexUpdate) {
            onIndexUpdate(data[firstIndex], firstIndex);
            onIndexUpdate(data[secondIndex], secondIndex);
        }
    }

    int left(int parent) const {
        int res = (parent << 1) + 1;
        if (res < data.size()) {
            return res;
        } else {
            return NO_HEAP_INDEX;
        }
    }

    int right(int parent) const {
        int res = (parent << 1) + 2;
        if (res < data.size()) {
            return res;
        } else {
            return NO_HEAP_INDEX;
        }
    }

    int parent(int child) const {
        if (child != ROOT_INDEX) {
            return (child - 1) >> 1;
        } else {
            return NO_HEAP_INDEX;
        }
    }

    static const int ROOT_INDEX = 0;
    Comparator comparator;
    std::function<void(const T&, int)> onIndexUpdate;
    std::vector<T> data;
};

typedef Heap<MemoryBlockIterator, MemoryBlockIteratorShorter> MemoryBlockIteratorHeap;

bool MemoryBlock::isFree() const  {
    return (heapIndex != MemoryBlockIteratorHeap::NO_HEAP_INDEX);
}


class MemoryManager {
public:
    explicit MemoryManager(unsigned int memorySize) {
        freeBlocks = Heap<MemoryBlockIterator,
                MemoryBlockIteratorShorter>(setHeapIndex<MemoryBlockIterator>);
        int noHeapIndex = MemoryBlockIteratorHeap::NO_HEAP_INDEX;
        MemoryBlockIterator initialBlockIterator = allBlocks.emplace(allBlocks.begin(), 0,
                memorySize, noHeapIndex);
        freeBlocks.insert(std::move(initialBlockIterator));
    }

    MemoryBlockIterator allocate(const unsigned int memoryBlockSize) {
        if (freeBlocks.empty()) {
            return end();
        }

        MemoryBlockIterator maxBlock = freeBlocks.getMax();
        if (maxBlock->size() < memoryBlockSize) {
            return end();
        }

        freeBlocks.pop();
        int begin = maxBlock->begin;
        int noHeapIndex = MemoryBlockIteratorHeap::NO_HEAP_INDEX;
        MemoryBlockIterator allocatedBlockIterator = allBlocks.emplace(maxBlock, begin,
                begin + memoryBlockSize, noHeapIndex);
        maxBlock->begin += memoryBlockSize;
        if (maxBlock->size() > 0) {
            freeBlocks.insert(std::move(maxBlock));
        } else {
            allBlocks.erase(maxBlock);
        }

        return allocatedBlockIterator;
    }

    void free(MemoryBlockIterator releasing) {
        if (releasing != allBlocks.begin()) {
            MemoryBlockIterator previousBlockIterator = std::prev(releasing);
            if (previousBlockIterator->isFree()) {
                releasing->begin = previousBlockIterator->begin;
                freeBlocks.remove(previousBlockIterator->heapIndex);
                allBlocks.erase(previousBlockIterator);
            }
        }

        if (std::next(releasing) != allBlocks.end()) {
            MemoryBlockIterator nextBlockIterator = std::next(releasing);
            if (nextBlockIterator->isFree()) {
                releasing->end = nextBlockIterator->end;
                freeBlocks.remove(nextBlockIterator->heapIndex);
                allBlocks.erase(nextBlockIterator);
            }
        }

        freeBlocks.insert(releasing);
    }
    MemoryBlockIterator end() {
        return allBlocks.end();
    }

private:
    Heap<MemoryBlockIterator, MemoryBlockIteratorShorter> freeBlocks;
    std::list<MemoryBlock> allBlocks;
};


struct MemoryManagerQuery {
    enum class Type {ALLOCATE_MEMORY, FREE_MEMORY};
    virtual Type getType() const = 0;
};


struct MemoryManagerAllocateQuery : MemoryManagerQuery {
    explicit MemoryManagerAllocateQuery(unsigned int memoryBlockSize) :
            memoryBlockSize(memoryBlockSize) {
    }
    virtual Type getType() const {
        return Type::ALLOCATE_MEMORY;
    }
    unsigned int memoryBlockSize;
};


struct MemoryManagerFreeQuery : MemoryManagerQuery {
    explicit MemoryManagerFreeQuery(unsigned int queryIndex) :
            queryIndex(queryIndex) {
    }
    virtual Type getType() const {
        return Type::FREE_MEMORY;
    }
    unsigned int queryIndex;
};


struct MemoryManagerAnswer {
    bool is_success;
    unsigned int memoryBlockBegin;
};


std::vector<MemoryManagerAnswer> processQueries(unsigned int memorySize,
        const std::vector<std::unique_ptr<MemoryManagerQuery>>& queries) {
    MemoryManager memoryManager(memorySize);
    std::vector<MemoryManagerAnswer> answers;
    answers.reserve(queries.size());
    std::vector<MemoryBlockIterator> allocated(queries.size());

    for (size_t counter = 0; counter < queries.size(); ++counter) {
        if (queries[counter].get()->getType() == MemoryManagerQuery::Type::ALLOCATE_MEMORY) {
            allocated[counter] = memoryManager.allocate(static_cast<MemoryManagerAllocateQuery*>
            (queries[counter].get())->memoryBlockSize);

            MemoryManagerAnswer answer;
            if (allocated[counter] != memoryManager.end()) {
                answer.is_success = true;
                answer.memoryBlockBegin = allocated[counter]->begin + 1;
            } else {
                answer.is_success = false;
            }
            answers.push_back(std::move(answer));
        } else {
            size_t index =
                    static_cast<MemoryManagerFreeQuery*>(queries[counter].get())->queryIndex;
            if (allocated[index] != memoryManager.end()) {
                memoryManager.free(allocated[index]);
            }
        }
    }

    return answers;
}

const size_t readCapacity(std::istream& inputStream = std::cin) {
    size_t capacity;
    inputStream >> capacity;
    return capacity;
}

std::vector<std::unique_ptr<MemoryManagerQuery>> readQueries(std::istream&
inputStream = std::cin) {
    size_t queriesCount;
    inputStream >> queriesCount;
    std::vector<std::unique_ptr<MemoryManagerQuery>> queries(queriesCount);
    int query;

    for (auto& item : queries) {
        inputStream >> query;
        if (query > 0) {
            item = std::unique_ptr<MemoryManagerAllocateQuery> (
                    new MemoryManagerAllocateQuery(query));
        } else {
            item = std::unique_ptr<MemoryManagerFreeQuery>(new MemoryManagerFreeQuery(-query - 1));
        }
    }

    return queries;
}

void printMemoryManagerAnswers(const std::vector<MemoryManagerAnswer>& answers,
        std::ostream& outputStream = std::cout) {
    for (const auto& item : answers) {
        if (item.is_success) {
            outputStream << item.memoryBlockBegin << '\n';
        } else {
            outputStream << "-1\n";
        }
    }
}


int main() {
    const size_t capacity = readCapacity();
    const auto queries = readQueries();

    std::vector<MemoryManagerAnswer> answers = processQueries(capacity, queries);

    printMemoryManagerAnswers(answers);

    return 0;
}
