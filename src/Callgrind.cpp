#include "Callgrind.h"

#if CXX_STD==11 || CXX_STD==14

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>
#include <cstring>
#include <tuple>


inline void ERROR_MSG( const std::string& msg )
{
    throw std::logic_error(msg);
}


// Split a string by spaces/tabs until endline
void split( const char *line, std::vector<std::string>& vec )
{
    vec.clear();
    while ( *line > 10 ) {
        while( *line <= 32 ) { line++; }
        int i=0;
        while( line[i] > 32 ) { i++; }
        vec.push_back(std::string(line,i));
        line += i;
    }
}


// Split a filename to the path and file
std::tuple<std::string,std::string> splitFilename( const std::string& filename )
{
    int k = -1;
    for (size_t i=0; i<filename.size(); i++) {
        if ( filename[i]==47 || filename[i]==92 )
            k = i;
    }
    std::string file, path;
    if ( k >= 0 ) {
        path = filename.substr(0,k);
        file = filename.substr(k+1);
    } else {
        file = filename;
    }
    return std::make_tuple(path,file);
};


// Structure to hold data about a function
struct function_structure {
    int calls;
    long int cost;
    std::vector<function_structure> subfunctions;
    function_structure(): calls(0), cost(0) {}
};


// Convert the object, file, function id to a unique id_struct_id
id_struct getID( int ob, int fl, int fn, std::map<std::tuple<int,int,int>,id_struct>& map )
{
    std::tuple<int,int,int> id = std::make_tuple(ob,fl,fn);
    if ( map.find(id)==map.end() )
        map.insert(std::pair<std::tuple<int,int,int>,id_struct>(id,id_struct::create_id(map.size()+1)));
    return map[id];
}


// Read the id (ob, fl, fn, etc) from the line
int getID( const char* line, std::map<int,std::string>& map )
{
    const char* ptr1 = strchr(line,'(');
    const char* ptr2 = strchr(line,')');
    int id = atoi(ptr1+1);
    if ( map.find(id)==map.end() ) {
        ptr1 = ptr2+1;
        while ( *ptr1==' ' && *ptr1>10 ) { ptr1++; }
        ptr2 = ptr1+1;
        while ( *ptr2>10 ) { ptr2++; }
        map.insert(std::pair<int,std::string>(id,std::string(ptr1,ptr2-ptr1)));
    }
    return id;
}
std::tuple<int,function_structure> getData( const char* line, FILE *fid, std::map<int,std::string>& map )
{
    int id = getID(line,map);
    function_structure data;
    std::vector<std::string> vec;
    while ( 1 ) {
        long int pos = ftell(fid);
        char line2[1024];
        char *rtn = fgets(line2,sizeof(line2),fid);
        if ( rtn==NULL )
            break;
        if ( line2[0] <= 10 )
            break;
        if ( strncmp(line2,"calls=",6)==0 ) {
            data.calls = atoi(line2+6);
            continue;
        } else if ( strchr(line2,'=')!=NULL ) {
            fseek(fid,pos,SEEK_SET);
            break;
        }
        split(line2,vec);
        data.cost += atol(vec[1].c_str());
    }  
    return std::make_tuple(id,data);
}


/***********************************************************************
* Load the results from a callgrind file                               *
***********************************************************************/
callgrind_results loadCallgrind( const std::string& filename, double tol )
{
    // Open the file to memory for reading
    FILE *fid = fopen(filename.c_str(),"rb");
    if (fid==NULL)
        ERROR_MSG("Error opening file: "+filename);
    // Read and process the data
    int ob=-1;
    int fl=-1;
    int ob2=-1;
    int fl2=-1;
    int fun_index = -1;
    std::string name;
    callgrind_results results;
    std::vector<callgrind_function_struct>& functions = results.functions;
    std::map<std::tuple<int,int,int>,id_struct> id_struct_map;
    long int global_cost = 0;
    while ( 1 ) {
        char line[1024];
        char *rtn = fgets(line,sizeof(line),fid);
        if ( rtn==NULL )
            break;
        if ( line[0] <= 10 )
            continue;
        if ( strncmp(line,"version:",8)==0 ) {
            // Read the version
        } else if ( strncmp(line,"pid:",4)==0 ) {
            // Read the process id
        } else if ( strncmp(line,"cmd:",4)==0 ) {
            // Read the command line
        } else if ( strncmp(line,"part:",5)==0 ) {
            // Read the part number
        } else if ( strncmp(line,"creator:",8)==0 ) {
            // Read the creator
        } else if ( strncmp(line,"desc:",5)==0 ) {
            // Description data
        } else if ( strncmp(line,"positions:",10)==0 ) {
            // Read the position info
        } else if ( strncmp(line,"summary:",8)==0 ) {
            // Read the summary cost
            global_cost = atol(&line[8]);
        } else if ( strncmp(line,"totals:",7)==0 ) {
            // Read the total cost
        } else if ( strncmp(line,"events:",7)==0 ) {
            // Read the events
        } else if ( strncmp(line,"ob=",3)==0 ) {
            // Read the object
            ob = getID(line,results.name_map);
        } else if ( strncmp(line,"fl=",3)==0 ) {
            // Read the filename
            fl = getID(line,results.name_map);
        } else if ( strncmp(line,"fn=",3)==0 ) {
            // Read the function
            ob2=ob;
            fl2=fl;
            function_structure data;
            int fn = -1;
            std::tie(fn,data) = getData(line,fid,results.name_map);
            fun_index = functions.size();
            functions.resize(fun_index+1);
            functions[fun_index].id = getID(ob,fl,fn,id_struct_map);
            functions[fun_index].obj = ob;
            functions[fun_index].file = fl;
            functions[fun_index].fun = fn;
            functions[fun_index].exclusive_cost = data.cost;
        } else if ( strncmp(line,"cob=",3)==0 ) {
            // Read the object for subfunction
            ob2 = getID(line,results.name_map);
        } else if ( strncmp(line,"cfi=",3)==0 ) {
            // Read the filename for subfunction
            fl2 = getID(line,results.name_map);
        } else if ( strncmp(line,"cfn=",3)==0 || strncmp(line,"fi=",3)==0 || strncmp(line,"fe=",3)==0  ) {
            // Read the subfunction
            function_structure data;
            int id;
            std::tie(id,data) = getData(line,fid,results.name_map);
            callgrind_subfunction_struct subfunction;
            subfunction.id = getID(ob2,fl2,id,id_struct_map);
            subfunction.calls = data.calls;
            subfunction.cost = data.cost;
            functions[fun_index].subfunctions.push_back(subfunction);
        } else {
            std::string msg = "Unknown line in callgrind file\n  " + std::string(line);
            ERROR_MSG(msg);
        }
    }
    for (size_t i=0; i<functions.size(); i++) {
        functions[i].inclusive_cost = functions[i].exclusive_cost;
        for (size_t j=0; j<functions[i].subfunctions.size(); j++)
            functions[i].inclusive_cost += functions[i].subfunctions[j].cost;
    }
    // Check for and combine duplicate entries
    std::sort(functions.begin(),functions.end());
    for (int i=(int)functions.size()-1; i>0; i--) {
        if ( functions[i].id==functions[i-1].id ) {
            functions[i-1].inclusive_cost += functions[i].inclusive_cost;
            functions[i-1].exclusive_cost += functions[i].exclusive_cost;
            functions[i-1].subfunctions.insert(functions[i-1].subfunctions.end(),
                functions[i].subfunctions.begin(),functions[i].subfunctions.end());
            for (size_t j=i; j<functions.size()-1; j++)
                std::swap(functions[j],functions[j+1]);
            functions.resize(functions.size()-1);
        }
    }
    // Check for duplicate subfunctions
    for (size_t i=0; i<functions.size(); i++) {
        std::vector<callgrind_subfunction_struct>& subfunctions = functions[i].subfunctions;
        std::sort(subfunctions.begin(),subfunctions.end());
        for (int j=(int)subfunctions.size()-1; j>0; j--) {
            if ( subfunctions[j].id==subfunctions[j-1].id ) {
                subfunctions[j-1].calls += subfunctions[j].calls;
                subfunctions[j-1].cost += subfunctions[j].cost;
                for (size_t k=j; k<subfunctions.size()-1; k++)
                    std::swap(subfunctions[k],subfunctions[k+1]);
                subfunctions.resize(subfunctions.size()-1);
            }
        }
    }
    // Check that the total cost adds correctly
    long int total_cost = 0;
    for (size_t i=0; i<functions.size(); i++)
        total_cost += functions[i].exclusive_cost;
    if ( std::abs(total_cost-global_cost) > 0.001*global_cost )
        printf("Warning: cost is not conserved loading callgrind files");
    // Delete functions whose cost is < tol*global cost or whose filename is an address
    if ( tol < 0 )
        ERROR_MSG("tol must be >=0");
    /*std::vector<bool> keep(functions.size(),true);
    for (size_t i=0; i<functions.size(); i++) {
        if ( functions[i].inclusive_cost < tol*global_cost )
            keep[i] = false;
        const std::string& fun = results.name_map[functions[i].fun];
        if ( fun[0]=='0' && fun[1]=='x' )
            keep[i] = false;
    }*/
    return results;
}


/***********************************************************************
* Convert callgrind results into timer results                         *
***********************************************************************/
std::vector<TimerResults> convertCallgrind( const callgrind_results& callgrind )
{
    std::map<id_struct,int> index_map;
    // Create the list of all timers
    std::vector<TimerResults> timers(callgrind.functions.size());
    for (size_t i=0; i<callgrind.functions.size(); i++) {
        std::string function = callgrind.name_map.find(callgrind.functions[i].fun)->second;
        std::string filename = callgrind.name_map.find(callgrind.functions[i].file)->second;
        timers[i].id = callgrind.functions[i].id;
        timers[i].message = function;
        std::tie(timers[i].path,timers[i].file) = splitFilename(filename);
        timers[i].start = -1;
        timers[i].stop = -1;
        index_map.insert(std::pair<id_struct,int>(timers[i].id,static_cast<int>(i)));
    }
    if ( timers.size() != index_map.size() )
        ERROR_MSG("Internal error");
    // Duplicate the functions, removing recursive functions
    std::vector<callgrind_function_struct> functions = callgrind.functions;
    for (size_t i=0; i<functions.size(); i++) {
        id_struct id = functions[i].id;
        std::vector<callgrind_subfunction_struct>& subfunctions = functions[i].subfunctions;
        for (size_t j=0; j<subfunctions.size(); j++) {
            if ( subfunctions[j].id==id ) {
                functions[i].exclusive_cost += subfunctions[j].cost;
                std::swap(subfunctions[j],subfunctions[subfunctions.size()-1]);
                subfunctions.resize(subfunctions.size()-1);
                std::sort(subfunctions.begin(),subfunctions.end());
            }
        }
    }
    // Create the traces
    std::map<id_struct,std::set<id_struct>> subfunctions;
    std::map<id_struct,std::set<id_struct>> parents;
    for (size_t i=0; i<functions.size(); i++) {
        id_struct id = functions[i].id;
        std::set<id_struct>& set = subfunctions[id];
        if ( parents.find(id) == parents.end() )
            parents[id] = std::set<id_struct>();
        for (size_t j=0; j<functions[i].subfunctions.size(); j++) {
            id_struct id2 = functions[i].subfunctions[j].id;
            set.insert(id2);
            parents[id2].insert(id);
        }
    }
    std::vector<std::pair<id_struct,std::set<id_struct>>> tmp;
    tmp.reserve(parents.size());
    for (const auto& it : parents ) {
        tmp.push_back(std::pair<id_struct,std::set<id_struct>>(it.first,it.second));
    }
    auto sort_fun = [](const std::pair<id_struct,std::set<id_struct>>& a,
                       const std::pair<id_struct,std::set<id_struct>>& b ) {
        return a.second.size() > b.second.size();
    };
    int it = 0;
    while ( !tmp.empty() ) {
        printf("iteration %i (%i)\n",it,(int)tmp.size());
        std::sort(tmp.begin(),tmp.end(),sort_fun);
        if ( tmp.rbegin()->second.empty() ) {
            // Go through the entries that do not have a parent
            for (int i=tmp.size()-1; i>=0&&tmp[i].second.empty(); i--) {
                // The current id is a new trace
                id_struct id = tmp[i].first;
                tmp.resize(i);
                int index = index_map[id];
                const callgrind_function_struct& fun = functions[index];
                double time = fun.inclusive_cost;
                for (size_t j=0; j<timers[index].trace.size(); j++)
                    time -= timers[index].trace[j].tot;
                if ( time > 0.01*fun.inclusive_cost ) {
                    // Create a new trace for self
                    TraceResults trace;
                    trace.id = id;
                    trace.min = 0.0;
                    trace.max = 0.0;
                    trace.tot = time;
                    trace.N = 1;
                    timers[index].trace.push_back(trace);
                }
                // For each subfunction, create the appropriate traces (and remove the parent)
                for (size_t j=0; j<fun.subfunctions.size(); j++) {
                    id_struct id2 = fun.subfunctions[j].id;
                    //int index2 = index_map[id2];
                    // Create the child traces assume the same ratio as the traces in the current timer
                    
                    // Erase the current timer from the parent list
                    for (size_t k=0; k<tmp.size(); k++) {
                        if ( tmp[k].first==id2 )
                            tmp[k].second.erase(id);
                    }
                }
            }
        } else {
            ERROR_MSG("Internal error: no parent");
        }
        it++;
    }
    return timers;

}


#else
    // Dummy implimentations for C++98
    callgrind_results loadCallgrind( const std::string& ) {
        return callgrind_results();
    }
    // Convert callgrind results into timers
    std::vector<TimerResults> convertCallgrind( const callgrind_results& ) {
        return std::vector<TimerResults>();
    }
#endif



