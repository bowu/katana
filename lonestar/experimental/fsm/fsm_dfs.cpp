#define USE_DFS
#define ENABLE_LABEL
#define EDGE_INDUCED
#define CHUNK_SIZE 4
#include "pangolin.h"

const char* name = "FSM";
const char* desc = "Frequent subgraph mining using DFS code";
const char* url  = 0;
 
class AppMiner : public EdgeMiner {
public:
	AppMiner(Graph *g, unsigned size) : EdgeMiner(g, size) {}
	~AppMiner() {}
	void print_output() {
		std::cout << "\n\ttotal_num_frquent_patterns = " << get_total_count() << "\n";
	}
};

#include "DfsMining/engine.h"

