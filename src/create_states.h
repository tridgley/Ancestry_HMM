#ifndef __CREATE_STATES_H
#define __CREATE_STATES_H

/// create the set of states that are permissable for a given ploidy
void create_initial_states ( double &number_chromosomes, vector<pulse> &ancestry_pulses, map<int,vector<vector<int> > > &state_list, bool gc) {
    
    /// if we have already computed the state list for this sample ploidy, move on
    if ( state_list.find( number_chromosomes ) != state_list.end() ) {
        return ;
    }
    
    /// list of all possible states for single chromosomes
    vector<int> states ;
    if (gc == false) {
        for ( int p = 0 ; p < ancestry_pulses.size() ; p ++ ) {
            states.push_back( p ) ;
        }
    } else {
        for ( int p = 0 ; p < 2*ancestry_pulses.size() ; p ++ ) {
            states.push_back( p ) ;
        }
    }
    
    /// now get all possible arrangements and store them in our state list
    vector< vector<int> > results = multichoose( number_chromosomes, states ) ;
    for ( int i = 0 ; i < results.size() ; i ++ ) {
        int state_vector_size;
        if (gc == false) {
            state_vector_size = ancestry_pulses.size();
        } else {
            state_vector_size = 2*ancestry_pulses.size();
        }
        vector<int> state_vector( state_vector_size, 0 ) ;
        for ( int j = 0 ; j < results[i].size() ; j ++ ) {
            state_vector.at(results[i][j]) ++ ;
        }
        state_list[number_chromosomes].push_back( state_vector ) ;
    }
}

#endif
