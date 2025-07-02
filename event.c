#include "sim.h"
#include <time.h>

void main(void)
{
    /*6 declare S for our simulation*/
	Simulation *S = (Simulation *) malloc (sizeof(Simulation));
    srand(time(NULL));
    InitSimulation(S);

    /* run the simulation */
    RunSimulation(S);
    
    /* print the results */
    PrintSimulationResults(S);
    system("pause");
}
