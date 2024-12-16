CXX = dpu-upmem-dpurte-clang

CXXFLAGS = --target=dpu-upmem-dpurte -fno-exceptions -fno-rtti -DNR_TASKLETS=$(TASKLETS) -DSTACK_SIZE_DEFAULT=$(STACK_SIZE) -DNDEBUG
CXXFLAGS += -O2 -std=c++20

TARGETS = short_read_mapping
STACK_SIZE = 3800
TASKLETS = 16  # You need to define TASKLETS value

all: $(TARGETS)

$(TARGETS): %: %.cpp
	$(CXX) $< -o $@ $(CXXFLAGS)

clean:
	rm -f $(TARGETS)
