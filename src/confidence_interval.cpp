//    LSD - Least Square Dating for etimating substitution rate and divergence dates
//    Copyright (C) <2015> <Thu-Hien To>

//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "confidence_interval.h"

void collapse(int i,int j,Pr* pr,Node** nodes,Node** nodes_new,int &cc,int* &tab){
    if (nodes[j]->type!='n') {
        nodes_new[i]->addConstraint(nodes[j]);
    }
    for (vector<int>::iterator iter=nodes[j]->suc.begin(); iter!=nodes[j]->suc.end(); iter++) {
        int s= *iter;
        if (s<pr->nbINodes && myabs(nodes[s]->B)<=toCollapse) {
            tab[s]=-1;
            collapse(i,s, pr, nodes, nodes_new, cc,tab);
        }
        else{
            nodes_new[s]->P=i;
            if (s<pr->nbINodes) {
                tab[s]=cc;
                cc++;
            }
        }
    }
}

int collapseTree(Pr* pr,Node** nodes,Node** nodes_new,int* &tab){
    for (int i=0;i<=pr->nbBranches;i++){
        nodes_new[i]= new Node();
        nodes_new[i]->P=nodes[i]->P;
        nodes_new[i]->type=nodes[i]->type;
        nodes_new[i]->lower=nodes[i]->lower;
        nodes_new[i]->upper=nodes[i]->upper;
        nodes_new[i]->D=nodes[i]->D;
        nodes_new[i]->B=nodes[i]->B;
    }
    int cc=0;//number of internal nodes reduced
    cc++;
    tab[0]=0;
    for (int i=0;i<pr->nbINodes;i++){
        if (myabs(nodes[i]->B)>toCollapse || i==0){
            for (vector<int>::iterator iter=nodes[i]->suc.begin(); iter!=nodes[i]->suc.end(); iter++) {
                int s=*iter;
                if (myabs(nodes[s]->B)<=toCollapse && s<pr->nbINodes) {
                    tab[s]=-1;
                    collapse(i, s,  pr, nodes, nodes_new,cc,tab);
                }
                else {
                    nodes_new[s]->P=i;
                    if (s<pr->nbINodes) {
                        tab[s]=cc;
                        cc++;
                    }
                }
            }
        }
    }
    for (int i=pr->nbINodes;i<=pr->nbBranches;i++){
        tab[i]=cc+i-pr->nbINodes;
    }
    return cc;
}

void collapseTreeReOrder(Pr* pr,Node** nodes,Pr* prReduced,Node** nodesReduced,int* &tab){
    nodesReduced[0]=new Node();
    nodesReduced[0]->P=-1;
    nodesReduced[0]->type=nodes[0]->type;
    nodesReduced[0]->lower=nodes[0]->lower;
    nodesReduced[0]->upper=nodes[0]->upper;
    nodesReduced[0]->D=nodes[0]->D;
    for (int i=1; i<=pr->nbBranches; i++) {
        if (tab[i]!=-1) {
            nodesReduced[tab[i]]=new Node();
            nodesReduced[tab[i]]->P=tab[nodes[i]->P];
            nodesReduced[tab[i]]->B=nodes[i]->B;
            nodesReduced[tab[i]]->type=nodes[i]->type;
            nodesReduced[tab[i]]->lower=nodes[i]->lower;
            nodesReduced[tab[i]]->upper=nodes[i]->upper;
            nodesReduced[tab[i]]->D=nodes[i]->D;
        }
    }
}

void computeIC(double br,Pr* pr,Node** nodes,double* &T_left,double* &T_right,double &rho_left,double& rho_right){
    Node** nodes_new = new Node*[pr->nbBranches+1];
    int* tab = new int[pr->nbBranches+1];
    int nbC = collapseTree(pr, nodes, nodes_new,tab);//nbC is the number of internal nodes reduced
    Node** nodesReduced = new Node*[nbC+pr->nbBranches-pr->nbINodes+1];
    Pr* prReduced = new Pr(nbC,nbC+pr->nbBranches-pr->nbINodes);
    prReduced->copy(pr);
    prReduced->init();
    collapseTreeReOrder( pr, nodes_new, prReduced, nodesReduced,tab);
    delete[] nodes_new;
    computeSuc_polytomy(prReduced, nodesReduced);
    double** B_simul = new double*[pr->nbSampling];
    for (int i=0;i<pr->nbSampling;i++){
        B_simul[i]=new double[prReduced->nbBranches+1];
    }
    srand ( time(NULL) );
    std::default_random_engine generator;
    double minB=nodesReduced[1]->B;
    for (int i=2;i<=prReduced->nbBranches;i++){
        if (nodesReduced[i]->B<minB && nodesReduced[i]->B>0) {
            minB=nodesReduced[i]->B;
        }
    }
    for (int i=1;i<=prReduced->nbBranches;i++){
        std::poisson_distribution<int> distribution(nodesReduced[i]->B*pr->seqLength);
        for (int j=0;j<pr->nbSampling;j++){
            B_simul[j][i]=(double)distribution(generator)/(pr->seqLength);
        }
    }
    double** T_simul = new double*[pr->nbSampling];
    double* rho_simul = new double[pr->nbSampling];
    std::poisson_distribution<int> distribution(br*pr->seqLength);
    for (int r=0;r<pr->nbSampling;r++){
        for (int j=0;j<=prReduced->nbBranches;j++){
            nodesReduced[j]->B=B_simul[r][j];
        }
        initialize_status(prReduced, nodesReduced);
        computeVariance(prReduced, nodesReduced);
        if (pr->constraint) with_constraint_active_set(prReduced,nodesReduced);
        else without_constraint_active_set(prReduced,nodesReduced);
        T_simul[r]=new double[prReduced->nbBranches+1];
        for (int j=0;j<=prReduced->nbBranches;j++) T_simul[r][j]=nodesReduced[j]->D;
        rho_simul[r]=prReduced->rho;
        //cout<<"Tree "<<r<<" : "<<prReduced->rho<<" "<<nodesReduced[0]->D<<endl;
        delete[] B_simul[r];
    }
    sort(rho_simul,pr->nbSampling);
    rho_left=rho_simul[int(0.025*pr->nbSampling)];
    rho_right=rho_simul[pr->nbSampling-int(0.025*pr->nbSampling)-1];
    if (pr->rho<rho_left) rho_left=pr->rho;
    if (pr->rho>rho_right) rho_right=pr->rho;
    double* T_sort = new double[pr->nbSampling];
    for (int i=0;i<=pr->nbBranches;i++){
        if (tab[i]!=-1) {
            for (int j=0;j<pr->nbSampling;j++) {
                T_sort[j]=T_simul[j][tab[i]];
            }
            sort(T_sort,pr->nbSampling);
            T_left[i]=T_sort[int(0.025*pr->nbSampling)];
            if (T_left[i]>nodes[i]->D) T_left[i]=nodes[i]->D;
            T_right[i]=T_sort[pr->nbSampling-int(0.025*pr->nbSampling)-1];
            if (T_right[i]<nodes[i]->D) T_right[i]=nodes[i]->D;
        }
    }
    delete[] rho_simul;
    delete[] T_sort;
    for (int i=0;i<pr->nbSampling;i++){
        delete[] T_simul[i];
    }
}

void output(double br,int y, Pr* pr,Node** nodes,FILE* f,FILE* tree1,FILE* tree2,FILE* tree3){
    if (pr->relative) {
        fprintf(f,"The results correspond to the estimation of relative dates when T[mrca]=%0.3f and T[tips]=%0.3f\n",pr->mrca,pr->leaves);
        printf("The results correspond to the estimation of relative dates when T[mrca]=%0.3f and T[tips]=%0.3f\n",pr->mrca,pr->leaves);
    }
    fprintf(f,"rate %.3e , tMRCA %.3f , objective function %.6e\n",pr->rho,nodes[0]->D,pr->objective);
    printf("rate %.3e , tMRCA %.3f , objective function %.6e\n",pr->rho,nodes[0]->D,pr->objective);
    if (pr->variance==2){
        printf("Re-estimating using variances based on the branch lengths of the previous run ...\n");
        fprintf(f,"Re-estimate using variances based on the branch lengths of the previous run ...\n");
        if (pr->estimate_root=="") {
            if (pr->constraint) with_constraint_active_set_secondTime(pr, nodes);
            else without_constraint_active_set_secondTime(pr, nodes);
        }
        else{
            if (pr->constraint) with_constraint_active_set_lambda_secondTime(br,pr, nodes);
            else without_constraint_active_set_lambda_secondTime(br,pr, nodes);
        }
        fprintf(f,"rate %.3e , tMRCA %.3f , objective function %.6e\n",pr->rho,nodes[0]->D,pr->objective);
        printf("rate %.3e , tMRCA %.3f , objective function %.6e\n",pr->rho,nodes[0]->D,pr->objective);
    }
    for (int i=1;i<=pr->nbBranches;i++){
        nodes[i]->B=pr->rho*(nodes[i]->D-nodes[nodes[i]->P]->D);
    }
    if (!pr->ci){
        fprintf(tree1,"tree %d = ",y);
        fprintf(tree1,"%s",nexus(0,pr,nodes).c_str());
        fprintf(tree2,"%s",newick(0,0,pr,nodes).c_str());
        fprintf(tree3,"%s",newickDate(0,pr,nodes).c_str());
        if (!pr->constraint){
            int count=0;
            for (int i=1;i<=pr->nbBranches;i++){
                if (nodes[i]->D<nodes[nodes[i]->P]->D) count++;
            }
            fprintf(f,"Number of violated temporal constraints (nodes having date smaller than the one of its parent): %d (%.2f%%)\n",count,(count*100)/(double)pr->nbBranches);
            printf("Number of violated temporal constraint (node having date smaller than the one of its parent): %d (%.2f%%)\n",count,(count*100)/(double)pr->nbBranches);
            if (count>0) {
                fprintf(f,"Try option -c to impose temporal constraints on the estimated trees\n");
                printf("Try option -c to impose temporal constraints on the estimated trees\n");
            }
        }
        fprintf(f,"\n");
        printf("\n");
    }
    else{
        if (!pr->constraint) {
            printf("WARNING: Confidence intervals are not warranted under non-constraint mode\n");
            fprintf(f,"WARNING: Confidence intervals are not warranted under non-constraint mode\n");
        }
        double* T_min = new double[pr->nbBranches+1];
        double* T_max = new double[pr->nbBranches+1];
        double rho_left,rho_right;
        cout<<"Computing confidence intervals ..."<<endl;
        computeIC(br,pr,nodes,T_min,T_max,rho_left,rho_right);
        fprintf(f,"rate %.3e [%.3e , %.3e] , tMRCA %.3f [%.3f , %.3f] , objective function %.6e\n",pr->rho,rho_left,rho_right,nodes[0]->D,T_min[0],T_max[0],pr->objective);
        printf("rate %.3e [%.3e , %.3e] , tMRCA %.3f [%.3f , %.3f] , objective function %.6e\n",pr->rho,rho_left,rho_right,nodes[0]->D,T_min[0],T_max[0],pr->objective);
        fprintf(tree1,"tree %d = ",y);
        fprintf(tree1,"%s",nexusIC(0,pr,nodes,T_min,T_max).c_str());
        fprintf(tree2,"%s",newick(0,0,pr,nodes).c_str());
        fprintf(tree3,"%s",newickDate(0,pr,nodes).c_str());
        if (!pr->constraint){
            int count=0;
            for (int i=1;i<=pr->nbBranches;i++){
                if (nodes[i]->D<nodes[nodes[i]->P]->D) count++;
            }
            fprintf(f,"Number of violated temporal constraints (nodes having date smaller than the one of its parent): %d (%.2f%%)\n",count,(count*100)/(double)pr->nbBranches);
            printf("Number of violated temporal constraints (node having date smaller than the one of its parent): %d (%.2f%%)\n",count,(count*100)/(double)pr->nbBranches);
            if (count>0) {
                fprintf(f,"Try option -c to impose temporal constraints on the estimated trees\n");
                printf("Try option -c to impose temporal constraints on the estimated trees\n");
            }
        }
        fprintf(f,"\n");
        printf("\n");
        delete[] T_min;
        delete[] T_max;
    }
    if (pr->rho==pr->rho_min) {
        fprintf(f, "The estimated rate reaches the given lower bound. To change the lower bound, use option -t\n");
        printf("The estimated rate reaches the given lower bound. To change the lower bound, use option -t\n");
    }
}
