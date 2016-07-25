#ifndef __LPSOLVER_H__
#define __LPSOLVER_H__

#include "gurobi_c++.h"
#include "splice_graph.h"

class smoother
{
public:
	smoother(splice_graph &g);
	virtual ~smoother();

private:
	splice_graph &gr;				// splice graph

	vector<VE> xeq;					// equations
	vector<VE> yeq;

	vector<edge_descriptor> i2e;	// edge map
	MEI e2i;						// edge map
	vector<int> v2i;				// vertex map
	vector<int> i2v;				// vertex map

	vector<GRBVar> vnwt;			// new weights for nodes
	vector<GRBVar> enwt;			// new weights for 
	vector<GRBVar> verr;			// error for nodes
	vector<GRBVar> eerr;			// error for edges

	GRBModel * model;
	GRBEnv * env;

public:
	int smooth();					// smooth the whole graph
	int smooth_vertex(int i);		// smooth only vertex i
	int add_equation(const VE &x, const VE &y);

private:
	int build_edge_map();
	int build_edge_map(int i);
	int build_vertex_map();
	int build_vertex_map(int i);

	int add_vertex_weight_variables();
	int add_vertex_error_variables();
	int add_edge_weight_variables();
	int add_edge_error_variables();

	int add_vertex_error_constraints();
	int add_edge_error_constraints();
	int add_edge_weight_constraints();

	int add_conservation_constraints();
	int add_additional_constraints();

	int set_objective();
	int update_weights();
};

#endif
