#include <iostream>
#include <vector>
#include <climits>
#include <algorithm>
#include <chrono>
#include "cnf-sat-vc.hpp"
#include <minisat/core/SolverTypes.h>
#include <minisat/core/Solver.h>

bool LOGGING_EN = false;


std::string printvect(std::vector<int> resultpaths) {
    std::string result;
    bool first = true;
    for (auto& v : resultpaths) {
        if (first) {
            first = false;
        }
        else {
            result += ",";
        }
        result += std::to_string(v);  
    }
    return result;
}
// Empty Constructor
VertexCover::VertexCover( int v, std::vector< std::pair<int,int> > e):
    vertices(v), edges(e) {}


// Private
Minisat::Var VertexCover::toVar(int row, int column, int k) {
    // Map all vertices to a SAT boolean problem
    // rows indicate vertices and columns indicate position in the minimum vertex cover solution
    
    int columns = k;
    return row * columns + column;
}


bool VertexCover::checkvalidinput(std::vector< std::pair<int,int> > edges) {
    for ( auto& e : edges) {
        if (vertices <= e.first || vertices <= e.second || e.first < 0 || e.second < 0) {
            std::cerr << "Error: Attempted to add edge to vertex that does not exist"
                      << std::endl;
            return false;
        }
        if (e.first == e.second) {
            std::cerr << "Error:  Cannot add edge from vertex to itself"
                      << std::endl;
            return false;
        }
    }
    
    return true;
}


void VertexCover::addclause_atleastone(Minisat::Solver& solver, int k) {
    // At least one vertex is the ith vertex in the vertex cover

    for (int i = 0; i < k; i++) {
        Minisat::vec<Minisat::Lit> clause;
        for (int n = 0; n < vertices; n++) {
            clause.push(Minisat::mkLit(toVar(n,i,k)));
        }
        solver.addClause(clause);
    }
}


void VertexCover::addclausevertexonlyonce(Minisat::Solver& solver, int k) {
    // No more than one vertex in the ith position of the vertex cover

    for (int i = 0; i < vertices; i++) {   
        for (int q = 0; q < k; q++) {
            for (int p = 0; p < q; p++) {
                solver.addClause( ~Minisat::mkLit(toVar(i,p,k)), ~Minisat::mkLit(toVar(i,q,k)));  
            }   
        }        
    }
}


void VertexCover::addclause_onepercoverpos(Minisat::Solver& solver, int k) {
    // No more than one vertex in the ith position of the vertex cover
    
    for (int i = 0; i < k; i++) {   
        for (int q = 0; q < vertices; q++) {
            for (int p = 0; p < q; p++) {
                solver.addClause( ~Minisat::mkLit(toVar(p,i,k)), ~Minisat::mkLit(toVar(q,i,k)));  
            }   
        }        
    }
}


void VertexCover::

addclause_alledgecovered(Minisat::Solver& solver, int k) {
    // Every edge is incident to at least one vertex
    for ( auto& e : edges) {
        Minisat::vec<Minisat::Lit> literals;
        for (int i = 0; i < k; i++) {
            literals.push(Minisat::mkLit(toVar(e.first, i, k)));
            literals.push(Minisat::mkLit(toVar(e.second, i, k)));
            }
        solver.addClause(literals);
        }
}


bool VertexCover::solve(Minisat::Solver& solver, int k) {
    
    for (int r = 0; r < vertices; r++) {
        for (int c = 0; c < k; c++) {
            solver.newVar();
            }
    }

    // add clauses
    addclause_atleastone(solver, k);
    addclausevertexonlyonce(solver, k);
    addclause_onepercoverpos(solver, k);
    

addclause_alledgecovered(solver, k);
    if (LOGGING_EN) {
        std::clog << " Num_Clauses=" << solver.nClauses();
    }

    auto sat = solver.solve();
    
    return sat;
}

std::vector<int> VertexCover::getpath(Minisat::Solver& solver, int k) {
    
    std::vector<int> path;
    
    for (int r = 0; r < vertices; r++) {
        for (int c = 0; c < k; c++) {
            if (solver.modelValue(toVar(r,c,k)) == Minisat::l_True) {
                path.push_back(r);
            }
        }      
    }
    std::sort(path.begin(), path.end());
    return path;
}

// Accessors
int VertexCover::getvertices() const {
    return vertices;
}


// Mutators
void VertexCover::addedges(std::vector< std::pair<int,int> > e) {
    if ( checkvalidinput(e) ) {
        edges = e;
    }
    return;
}


std::string VertexCover::linsearch_VC() {
    if (!checkvalidinput(edges)) {
        return "";
    }
   
    if (edges.empty()) {
        return "";
    }
    
    // Finds the minimum vertex cover and prints path
    // Using Linear search to find minimum
        
    int results[vertices];  //0 is UNSAT, 1 is SAT, -1 is undefined where index is k or vertex cover length
    std::vector<int> resultpaths[vertices];
    std::fill_n(results, vertices, -1);

    for (int i=0; i <vertices; i++) {
        Minisat::Solver solver;
        
        if (LOGGING_EN) {
            std::clog << "Trying K=" << i;   
        }
        auto start = std::chrono::system_clock::now(); // start measuring time
        
        results[i] = solve(solver, i);
        
        
        auto end = std::chrono::system_clock::now();   // stop measuring time
        std::chrono::duration<double> diff = end-start;
        if (LOGGING_EN) {    
            std::clog << " Result: " << results[i] << " Duration=" << diff.count() << std::endl;
        }
        
        if (results[i] and !results[i-1]) {
            resultpaths[i] = getpath(solver, i);
            return printvect(resultpaths[i]);
        }
    }

    std::cerr << "Error: UNSAT" << std::endl;
    return "";
}

std::string VertexCover::binsearch_VC() {
    if (!checkvalidinput(edges)) {
        return "";
    }

    if (edges.empty()) {
        return "";
    }
    
    // Finds the minimum vertex cover and prints path
    // Using Binary search to find minimum
    int low = 0;
    int high = vertices;
    int mid;
        
    int results[vertices];  //0 is UNSAT, 1 is SAT, -1 is undefined where index is k or vertex cover length
    std::vector<int> resultpaths[vertices];
    std::fill_n(results, vertices, -1);

    while (low <= high) {
        mid = (high+low)/2;
        Minisat::Solver solver;

        if (LOGGING_EN) {
            std::clog << "Trying K=" << mid;
        }
        auto start = std::chrono::system_clock::now();  // start measuring time

        results[mid] = solve(solver, mid);

        auto end = std::chrono::system_clock::now();    // stop measuring time
        std::chrono::duration<double> diff = end-start;
        if (LOGGING_EN) {
            std::clog << " Result: " << results[mid] << " Duration=" << diff.count() << std::endl;
        }

        if (results[mid]) {
            resultpaths[mid] = getpath(solver, mid);
        }
          
        // If SAT and result[k-1] are UNSAT, the minimum is found
        if (results[mid] == 1 && results[mid-1] == 0 && mid != 0) {
            return printvect(resultpaths[mid]);
        }

        // If UNSAT and result[k+1] are SAT, the minimum is found
        if (results[mid] == 0 && results[mid+1] == 1 && mid != vertices) {
            return printvect(resultpaths[mid+1]);
        }
        
        if (results[mid]) {
            high = mid - 1;
        } 
        else {
            low = mid + 1;
        }
    }

    std::cerr << "Error: UNSAT" << std::endl;
    return "";
}
