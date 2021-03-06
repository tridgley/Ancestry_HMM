#ifndef __CREATE_TRANSITION_RATES_H
#define __CREATE_TRANSITION_RATES_H

//// create transition matrix for a given admixture model
void create_transition_matrix ( map<int,vector<mat> > &transition_matrix , vector<vector< map< vector<transition_information>, double > > > &transition_info, vector<double> &recombination_rate, vector<int> &positions, double &number_chromosomes, mat &transition_rates) {
    
    /// check if we already computed this for this sample ploidy
    if ( transition_matrix.find( number_chromosomes ) != transition_matrix.end() ) {
        return ;
    }
    
    /// else, have to create entire matrix
    /// first create data object of approporate size
    transition_matrix[number_chromosomes].resize(recombination_rate.size()) ;
    
    //// iterate across all positions and compute transition matrixes
    for ( int p = 1 ; p < recombination_rate.size() ; p ++ ) {
        
        /// create actual transition matrix
        transition_matrix[number_chromosomes][p].set_size( transition_info.size(), transition_info.size() ) ;
        transition_matrix[number_chromosomes][p].fill( 0 ) ;
        
        /// if onto a new chromosome
        if ( recombination_rate[p] > 0.4 ) {
            transition_matrix[number_chromosomes][p].fill(1/double(transition_info.size())) ;
            continue ;
        }
        
        /// otherwise normal transition rates
        /// create matrix of per bp changes
        mat site_transitions = transition_rates * recombination_rate[p] ;
        
        /// now extract the sum of all non-self transitions in a row, and take remainder
        for ( int i = 0 ; i < transition_rates.n_rows ; i ++ ) {
            double sum = 1 ;
            for ( int j = 0 ; j < transition_rates.n_rows ; j ++ ) {
                if ( j != i ) {
                    sum -= site_transitions(i,j) ;
                }
            }
            site_transitions(i,i) = sum ;
        }
        
        /// create final transition matrix via exponentiation by squaring
        mat segment_transitions ;
        if ( recombination_rate[p] > 0.49 ) {
            segment_transitions = exp_matrix( site_transitions, 2 ) ;
        }
        else {
            segment_transitions = exp_matrix( site_transitions, positions[p] - positions[p-1] ) ;
        }
                
        /// population transitions by summing across all routes
        for ( int i = 0 ; i < transition_info.size() ; i ++ ) {
            for ( int j = 0 ; j < transition_info[i].size() ; j ++ ) {
                for ( std::map<vector<transition_information>,double>::iterator t = transition_info[i][j].begin() ; t != transition_info[i][j].end() ; ++ t ) {
                    double prob_t = 1 ;
                    for ( int r = 0 ; r < t->first.size() ; r ++ ) {
                        prob_t *= pow( segment_transitions(t->first[r].start_state,t->first[r].end_state), t->first[r].transition_count ) ;
                    }
                    transition_matrix[number_chromosomes][p](j,i) += prob_t * t->second ;
                }
            }
        }
    }
    cout << "transition_info.size() " << transition_info.size() << endl;
    cout << "transition_info[0].size() " << transition_info[0].size() << endl;
    std::map<vector<transition_information>,double>::iterator t = transition_info[1][0].begin() ;
    // cout << "(TR_LOG) transition_info[0][0][t].size() " << t->first.size() << t->second << endl;
    for( int i = 0; i < t->first.size(); ++i ) {
        cout << "transition info index " << i << " start_state: " << t->first[i].start_state << endl;
        cout << "transition info index " << i << " end_state: " << t->first[i].end_state << endl;
        cout << "transition info index " << i << " transition_count: " << t->first[i].transition_count << endl;
        cout << "transition info index " << i << " ibd_transition: " << t->first[i].ibd_transition << endl;
    }
}

//// create all transition rates between ancestry types for a single chromosome
mat create_transition_rates ( vector<pulse> admixture_pulses, double n, vector<double> ancestry_proportion, bool gc, double gc_mean_dist, double gc_frac) {
    
    /// determine ancestry proportions based on the fraction of remainder
    for ( int p = 0 ; p < admixture_pulses.size() ; p ++ ) {
        admixture_pulses[p].proportion = admixture_pulses[p].fraction_of_remainder * ancestry_proportion[admixture_pulses[p].type] ;
        ancestry_proportion[admixture_pulses[p].type] -= admixture_pulses[p].proportion ;
    }
    
    //// sort by time so oldest events are at the top
    sort( admixture_pulses.begin(), admixture_pulses.end() ) ;
    
    /// all ancestry proprionts to this point are in units of final ancestry
    /// however before the pulses that came before, there was more
    double ancestry_accounted = 0 ;
    for ( int p = admixture_pulses.size() - 1 ; p > 0 ; p -- ) {
        double accounted_here = admixture_pulses[p].proportion ;
        admixture_pulses[p].proportion = admixture_pulses[p].proportion/( 1 - ancestry_accounted ) ;
        ancestry_accounted += accounted_here ;
    }

    /// matrix to hold transition rates
    int transition_rows;
    int transition_cols;
    transition_rows = admixture_pulses.size();
    transition_cols = admixture_pulses.size();
    mat transition_rates( transition_rows, transition_cols, fill::zeros ) ;
    
    /// iterate through all states
    int anc_pulses;
    if( gc == false ) {
        anc_pulses = admixture_pulses.size();
    } else {
        anc_pulses = admixture_pulses.size()/2;
    }
    for ( int s1 = 0 ; s1 < anc_pulses ; s1 ++ ) {
        for ( int s2 = 0 ; s2 < anc_pulses ; s2 ++ ) {
            
            //// self transition rates are just going to be 1-all others
            if ( s2 == s1 ) continue ;
            
            /// rates calculated one way if greater than
            if ( s1 > s2 ) {
                for ( int t = s1 ; t < anc_pulses ; t ++ ) {
                    
                    /// basic recombination rates in each epoch
                    double rate ;
                    if ( t != anc_pulses - 1 ) {
                        rate = n * ( 1 - exp( (admixture_pulses[t+1].time-admixture_pulses[t].time)/n ) ) ;
                    }
                    else {
                        rate = n * ( 1 - exp( (-1*admixture_pulses[t].time)/n ) ) ;
                    }
                    
                    /// probability of no recombination in prior epochs
                    /// will skip epoch of s1 since that's in the above equation by definition
                    for ( int t2 = t - 1 ; t2 > s1 - 1 ; t2 -- ) {
                        rate *= exp((admixture_pulses[t2+1].time-admixture_pulses[t2].time)/n) ;
                    }
                    
                    /// now probability of not selecting other acnestry types
                    for ( int a = t ; a > s2 ; a -- ) {
                        rate *= ( 1 - admixture_pulses[a].proportion ) ;
                    }
                    
                    /// now select the correct ancestry type
                    if ( s2 != 0 ) {
                        rate *= admixture_pulses[s2].proportion ;
                    }
                    transition_rates(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order) += rate ;
                }
                // Add the gene conversion transition rates below diag...
                if (gc == true && anc_pulses == 2 && transition_rates(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order) > 0) {
                    // Rate into a gene conversion tract
                    transition_rates(admixture_pulses[s2].entry_order + anc_pulses, admixture_pulses[s1].entry_order) = (gc_frac / (1-gc_frac)) * transition_rates.at(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order);
                    // Rate out of a GC_1 tract to ancestry 0
                    transition_rates(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order + anc_pulses) = 1 / gc_mean_dist + transition_rates.at(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order);
                    // Rate out of a GC_0 tract to ancestry 0
                    transition_rates(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order + 1) = transition_rates.at(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order);
                }
            }
            else {
                for ( int t = s2 ; t < anc_pulses ; t ++ ) {
                    
                    /// basic recombination rates in each epoch
                    double rate ;
                    if ( t != anc_pulses - 1 ) {
                        rate = n * ( 1 - exp( (admixture_pulses[t+1].time-admixture_pulses[t].time)/n ) ) ;
                    }
                    else {
                        rate = n * ( 1 - exp( (-1*admixture_pulses[t].time)/n ) ) ;
                    }
                    
                    /// probability of no recombination in prior epochs
                    /// will skip epoch of s1 since that's in the above equation by definition
                    for ( int t2 = t - 1 ; t2 > s2 - 1 ; t2 -- ) {
                        rate *= exp((admixture_pulses[t2+1].time-admixture_pulses[t2].time)/n) ;
                    }
                    
                    /// now deal with selecting lineage of the correct ancestry type
                    for ( int a = t ; a > s2 ; a -- ) {
                        rate *= ( 1 - admixture_pulses[a].proportion ) ;
                    }
                    
                    /// now select the correct ancestry type
                    rate *= admixture_pulses[s2].proportion ;
                    
                    /// and augment by this rate for this epoch
                    transition_rates(admixture_pulses[s2].entry_order,admixture_pulses[s1].entry_order) += rate ;
                }
                // Add the gene conversion rates above diag...
                if (gc == true && anc_pulses == 2 && transition_rates(admixture_pulses[s2].entry_order,admixture_pulses[s1].entry_order) > 0) {
                        // Transition rate into a gene conversion tract.
                        transition_rates(admixture_pulses[s2].entry_order + anc_pulses, admixture_pulses[s1].entry_order) = (gc_frac / (1-gc_frac)) * transition_rates.at(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order);
                        // Transition rate out of a GC0 tract to ancestry 1.
                        transition_rates(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order + anc_pulses) = 1 / gc_mean_dist + transition_rates.at(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order);
                        // Transition rate out of a GC tract to same ancestry.
                        transition_rates(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order + anc_pulses + 1) = transition_rates.at(admixture_pulses[s2].entry_order, admixture_pulses[s1].entry_order);
                }
            }
        }
    }
        
    return transition_rates.t() ;
}

#endif

