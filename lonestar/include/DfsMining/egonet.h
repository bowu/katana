#ifndef EGONET_H
#define EGONET_H
#include "types.h"
#ifdef ALGO_EDGE
#define BOTTOM 1
#else
#define BOTTOM 2
#endif

class Egonet {
public:
	Egonet() {}
	Egonet(unsigned c, unsigned k) {
		allocate(c, k);
	}
	~Egonet() {}
	void allocate(unsigned c, unsigned k) {
		core = c;
		max_size = k;
		degrees.resize(k);
		for (unsigned i = BOTTOM; i < k; i ++) degrees[i].resize(core);
		adj.resize(core*core);
	}
	unsigned get_adj(unsigned vid) { return adj[vid]; }
	unsigned getEdgeDst(unsigned vid) { return adj[vid]; }
	unsigned get_degree(unsigned level, unsigned i) { return degrees[level][i]; }
	void set_adj(unsigned vid, unsigned value) { adj[vid] = value; }
	void set_degree(unsigned level, unsigned i, unsigned degree) { degrees[level][i] = degree; }
	void inc_degree(unsigned level, unsigned i) { degrees[level][i] ++; }
	//unsigned get_vertex(unsigned level, unsigned i) { return emb_list.get_vertex(level, i); }
	//void set_vertex(unsigned level, unsigned i, unsigned value) { emb_list.set_vertex(level, i, value); }
	unsigned edge_begin(unsigned vid) { return vid * core; }

protected:
	unsigned core;
	unsigned max_size;
	UintList adj;//truncated list of neighbors
	IndexLists degrees;//degrees[level]: degrees of the vertices in the egonet
};

typedef galois::substrate::PerThreadStorage<Egonet> Egonets;

#endif
