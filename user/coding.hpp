#ifndef _CODING_HPP
#define _CODING_HPP

#include <stdint.h>
#include <vector>
#include <cstdio>

struct Coding{
	int nbit;
	uint32_t c;
	void add_bit(int bit){
		c = c | (bit << nbit);
		nbit++;
	}
	void clear(){
		nbit = 0;
		c = 0;
	}
};

struct Node{
	union{
		uint32_t freq;
		Coding c;
	};
	Node *son[2];
};

static void heap_down(std::vector<Node*> &h, int i){
	Node* x = h[i];
	while (i * 2 + 1 < h.size()){
		int j = i * 2 + 1;
		if (j+1 < h.size() && h[j+1]->freq < h[j]->freq)
			j++;
		if (h[j]->freq < x->freq){
			h[i] = h[j];
			i = j;
		}else 
			break;
	}
	h[i] = x;
}

static std::vector<Coding> huffman(std::vector<uint64_t> a){
	std::vector<Coding> res(a.size());
	if (a.size() <= 1){
		if (a.size())
			res.push_back((Coding){0,0});
		return res;
	}
	std::vector<Node> nodes(a.size() * 2);
	std::vector<Node*> heap(a.size());
	for (int i = 0; i < (int)a.size(); i++){
		nodes[i].freq = a[i];
		nodes[i].son[0] = nodes[i].son[1] = 0;
		heap[i] = &nodes[i];
	}

	// build binary heap
	for (int i = (int)a.size()/2 - 1; i >= 0; i--)
		heap_down(heap, i);
	// build huffman tree
	for (int i = (int)a.size() - 1, j = a.size(); i >= 0; i--, j++){
		// get min
		Node* min0 = heap[0], *min1;
		// update heap
		heap[0] = heap[i];
		heap.pop_back();
		heap_down(heap, 0);
		// get 2nd min
		min1 = heap[0];
		// create a new node
		nodes[j].son[0] = min0;
		nodes[j].son[1] = min1;
		nodes[j].freq = min0->freq + min1->freq;
		// insert the new node to heap
		heap[0] = &nodes[j];
		heap_down(heap, 0);
	}

	// encode
	nodes[(int)a.size() * 2 - 2].c.clear();
	for (int i = (int)a.size() * 2 - 2; i >= 0; i--){
		if (!nodes[i].son[0])
			continue;
		nodes[i].son[0]->c = nodes[i].c;
		nodes[i].son[0]->c.add_bit(0);
		nodes[i].son[1]->c = nodes[i].c;
		nodes[i].son[1]->c.add_bit(1);
	}
	for (int i = 0; i < a.size(); i++)
		res[i] = nodes[i].c;
	return res;
}

#endif /* _CODING_HPP */
