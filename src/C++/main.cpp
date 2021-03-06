#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iterator>
#include <algorithm>
#include <stdexcept>

#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/class.hpp>
#include <boost/python/numpy.hpp>

namespace p = boost::python;
namespace np = boost::python::numpy;

#include "mcmf.cpp"

void PrintVector(const std::vector<int> & vec){
  for (auto citer = vec.cbegin(); citer < vec.cend(); citer++){
    std::cout << *citer << " ";
  }
  std::cout << std::endl;
}

std::vector<int> Unique(std::vector<int> vec){
  std::sort(vec.begin(), vec.end());
  std::vector<int>::iterator it;
  it = std::unique (vec.begin(), vec.end());
  vec.resize( std::distance(vec.begin(),it) );
  return vec;
}

int NumberOfLines(std::ifstream & file, char ignore = '#'){
  std::string line;
  int count = 0;
  if (file.is_open()){
    while ( getline(file,line) ){
      if (line.length() >= 1){
        if (*line.begin() != ignore){
          count += 1;
        }
      }
    }
    file.clear();
    file.seekg(0, ios::beg);
    return count;
  }
  else{
    std::cout << "File read error\n";
    return -1;
  }
}

int Characters(std::string & str){
  int count = 0;
  for (auto citer = str.cbegin(); citer < str.cend(); citer++){
    if (std::isspace(*citer)){
      // count each space
      count += 1;
    }
  }
  // add the last nunmber
  count += 1;
  return count;
}

std::vector<int> ReadData(std::string & str, MCMF_CS2 & mcmf){
  bool arc;
  std::vector<int> values;
  if (Characters(str) == 2){
    arc = 0;
  }
  else{
    arc = 1;
  }

  std::string CurrNum;
  for (auto citer = str.cbegin(); citer <= str.cend(); citer++){
    if (std::isspace(*citer) || citer == str.cend()){
      values.push_back(std::stoi(CurrNum));
      CurrNum.clear();
    }
    else{
      CurrNum += *citer;
    }
  }
  if (arc == 1){
    mcmf.set_arc(values[0], values[1], values[2], values[3], values[4]);
  }
  else{
    mcmf.set_supply_demand_of_node(values[0], values[1]);
  }
  return values;
}

std::vector< std::vector<int> > ProblemFromFile(MCMF_CS2 & mcmf, std::ifstream & file){
  // takes an empty MCMF_CS2 object e.g MCMF_CS2 mcmf(0,0);
  // and a file of the form example_problem.in
  std::string line;
  // source and sink id
  std::vector<int> values = {0,0,-1,-1,-1};
  std::vector< std::vector<int> > returns;

  bool ReadNodeNumber = 0;
  if (file.is_open()){
    int Arcs = NumberOfLines(file)-3;
    while ( getline(file,line) ){
      if (line.length() >= 1){
        if (*line.begin() != '#'){
          if (ReadNodeNumber == 0){
            int Nodes = std::stoi(line);
            MCMF_CS2 m(Nodes, Arcs);
            mcmf = m;
            ReadNodeNumber = 1;
          }
          else{
            std::vector<int> vec = ReadData(line, mcmf);
            if (vec.size() == 2){
              if (vec[1] > 0){
                values[0] = vec[0];
              }
              else{
                values[1] = vec[0];
              }
              returns.insert(returns.begin(),values);
            }
            else{
              returns.push_back(vec);
            }
          }
        }
      }
    }
    file.close();
    return returns;
  }
  else{
    std::cout << "File read error\n";
    return returns;
  }
}

class MCMFProblem{
public:
  MCMFProblem(int n, np::ndarray arcs, np::ndarray source_sink)
    : mcmf(0,0), min_cost(1000000), flow(10), nodes(n)
    {
      try{
        if ( arcs.shape(0) <= 0 || arcs.shape(1) != 5 || source_sink.shape(0) < 2 ){
          throw std::runtime_error("Invalid input");
        }
        else{
          GetArcs(arcs);
          this->source_sink_id[0] = p::extract<int>(source_sink[0]);
          this->source_sink_id[1] = p::extract<int>(source_sink[1]);
        }
      }
      catch(std::runtime_error e){
        std::cout << e.what();
      }
    }
  MCMFProblem(long nodes, long arcs)
    : mcmf(nodes,arcs), min_cost(1000000), flow(10)
    {}
  MCMFProblem()
    : mcmf(0,0), min_cost(1000000), flow(10)
    {}
  void ReadFile(const std::string & filename);
  void SetFlow(int & flow){
    if (source_sink_id[0] > 0 && source_sink_id[1] > 0){
      this->mcmf.set_supply_demand_of_node(source_sink_id[0], flow);
      this->mcmf.set_supply_demand_of_node(source_sink_id[1], -flow);
    }
    else{
      std::cout << "Uninitialised source and sink\n";
      std::cout << "Or node label 0 is invalid start with 1";
    }
  }
  int Solve(bool debug, bool write_ans, std::string & out, int & min_cost){
    return this->mcmf.run_cs2(debug, write_ans, out, min_cost);
  }
  int Solve(int & current_min, int & cost){
    return this->mcmf.run_cs2_python(current_min, cost);
  }
  void TrajectoryAlgorithm(std::string & in, std::string & out, int flow_step = 10);
  np::ndarray PythonTrajectories(int flow_step, int print_status_every, bool store_costs, int initial_flow);
  np::ndarray Costs();
  int GetFinalFlow(){
    return this->final_flow;
  }

protected:
  //std::vector<int> ReadData(std::string & line);
  void LoadFromFile(std::ifstream & file){
    std::vector< std::vector<int> > data = ProblemFromFile(this->mcmf,file);
    this->nodes = this->mcmf._n;
    this->edges = this->mcmf._m;
    this->source_sink_id[0] = data[0][0];
    this->source_sink_id[1] = data[0][1];
    data.erase(data.begin());
    data.erase(data.begin());
    this->Arcs = data;
  }
  std::vector< std::vector<int> > Arcs;
  void GetArcs(np::ndarray & py_arcs);
  std::vector<int> source_sink_id = {0,0};
  int flow;
  int min_cost;
  int final_flow = -1;
  int cost;
  bool read_data = false;
  int edges;
  int nodes;
  std::vector< std::vector<int> > costs;
  MCMF_CS2 mcmf;
};

void MCMFProblem::GetArcs(np::ndarray & py_arcs){
  this->edges = py_arcs.shape(0);
  std::vector< std::vector<int> > Arcs(py_arcs.shape(0), std::vector<int>(5));
  this->Arcs = Arcs;
  for (int i = 0; i < py_arcs.shape(0); i++){
    for (int j = 0; j < py_arcs.shape(1); j++){
      this->Arcs[i][j] = p::extract<int>(py_arcs[i][j]);
    }
  }
}

np::ndarray MCMFProblem::Costs(){
    p::tuple shape = p::make_tuple(this->costs.size(),2);
    np::dtype dtype = np::dtype::get_builtin<int>();
    np::ndarray py_array = np::empty(shape,dtype);

    for (unsigned i = 0; i < this->costs.size(); i++){
      for (unsigned j = 0; j < this->costs[i].size(); j++){
        py_array[i][j] = this->costs[i][j];
      }
    }
    return py_array;
}

void MCMFProblem::TrajectoryAlgorithm(std::string & in,std::string & out, int flow_step){
  std::ifstream file;
  file.open(in);
  LoadFromFile(file);
  // std::ofstream out_file;
  // out_file.open("mcmf.out");
  // termination is when the problem is
  // unfeasible which cause the whole program to exit
  int flow = 0;
  while(true){
    SetFlow(flow);
    Solve(false,true,out,min_cost);
    file.open(in);
    LoadFromFile(file);
    flow += flow_step;
    //std::cout << flow << std::endl;
  }
}

np::ndarray MCMF_CS2::python_solution(int edges)
{
  p::tuple shape = p::make_tuple(edges,3);
  np::dtype dtype = np::dtype::get_builtin<int>();
  np::ndarray py_array = np::empty(shape,dtype);

	NODE *i;
	ARC *a;
	long ni;
	price_t cost;
  // surely this is unitilialised? therefor an undefined
	// operation?
	// printf ("c\ns %.0l\n", cost );
  int k = 0;
	for ( i = _nodes; i < _nodes + _n; i ++ ) {
		ni = N_NODE( i );
		for ( a = i->suspended(); a != (i+1)->suspended(); a ++) {
			if ( _cap[ N_ARC (a) ]  > 0 ) {
        py_array[k][0] = ni;
        py_array[k][1] = N_NODE(a->head());
        py_array[k][2] = _cap[ N_ARC(a) ] - a->rez_capacity();
        k++;
			}
		}
  }
  return py_array;
}

np::ndarray MCMFProblem::PythonTrajectories(int flow_step = 10, int print_status_every = 0, bool store_costs = false, int initial_flow = 0){
  if (print_status_every != 0){
    std::cout << "Printing Every " << print_status_every << std::endl << std::endl;
  }
  else{
    std::cout << "Running in silent mode\n";
  }
  p::tuple shape = p::make_tuple(this->edges,3);
  np::dtype dtype = np::dtype::get_builtin<int>();
  np::ndarray Trajectories = np::empty(shape,dtype);
  int flow = initial_flow;
  // this is a convex optimisation
  int change = 0;
  int prev_cost = 0;
  while(true){
    MCMF_CS2 m(this->nodes,this->edges);
    this->mcmf = m;
    for (int i = 0; i < this->edges; i++){
      std::vector<int> Arc = this->Arcs[i];
      this->mcmf.set_arc(Arc[0],Arc[1],Arc[2],Arc[3],Arc[4]);
    }
    SetFlow(flow);
    int state = 0;
    try{
      state = Solve(this->min_cost, this->cost);
      change = this->cost-prev_cost;
      if (state == 1){
        Trajectories = this->mcmf.python_solution(this->edges);
    		// () cleanup;
    		this->mcmf.deallocate_arrays();
      }
      if (change > 0){
        std::cout << "Reached the convex minimum\n";
        this->final_flow = flow-flow_step;
        return Trajectories;
      }
      prev_cost = this->cost;
    }
    catch(std::runtime_error & e){
  		// () cleanup;
      std::cout << e.what();
  		this->mcmf.deallocate_arrays();
      return Trajectories;
    }
    if (print_status_every != 0 && (flow % print_status_every) == 0){
      std::cout << "Current Flow: " << flow << " Current Min Cost: " << this->min_cost << std::endl;
      if (store_costs){
        std::vector<int> v = {this->cost,flow};
        this->costs.push_back(v);
      }
    }
    flow += flow_step;
  }
  return Trajectories;
}

void MCMFProblem::ReadFile(const std::string & filename){
  std::ifstream file;
  file.open(filename);
  if (file.is_open()){
    LoadFromFile(file);
  }
  else{
    std::cout << "Read error";
  }
}

using namespace boost::python;
BOOST_PYTHON_MODULE(mcmf_ext)
{
  np::initialize();
  class_<MCMFProblem>("MCMFProblem", init<int,np::ndarray,np::ndarray>())
    .def(init<long, long>())
    .def(init<>())
    .def("ReadFile", &MCMFProblem::ReadFile)
    .def("Trajectories", &MCMFProblem::PythonTrajectories)
    .def("Costs", &MCMFProblem::Costs)
    .def("GetFinalFlow", &MCMFProblem::GetFinalFlow)
  ;
}
