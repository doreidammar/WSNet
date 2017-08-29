/**
 *  \file   indoor.c
 *  \brief  ITU Indoor Propagation Model
 *  \author Doreid Ammar
 *  \date   2009
 **/
#include <include/modelutils.h>


/* ************************************************** */
/* ************************************************** */
model_t model =  {
    "ITU-R Indoor Propagation Model multi-protocol",
    "Doreid Ammar",
    "0.1",
    MODELTYPE_PROPAGATION, 
    {NULL, 0}
};

/* ************************************************** */
/* ************************************************** */
struct entitydata {
    double frequency_w;		/* The carrier frequency in MHz for the protocol_wifi */
    double distpower_lc_w;	/* The distance power loss coeficient for the protocol_wifi */
    int n_floors_w;		/* The number of floors penetrated for the protocol_wifi */
    double Lf_w;		/* The floor penetration loss factor (dB) for the protocol_wifi */
    double cst_w;
    double frequency_z;		/* The carrier frequency in MHz for the protocol_zigbee */
    double distpower_lc_z;	/* The distance power loss coeficient for the protocol_zigbee */
    int n_floors_z;		/* The number of floors penetrated for the protocol_zigbee */
    double Lf_z;		/* The floor penetration loss factor (dB) for the protocol_zigbee */
    double cst_z;
};

/* ************************************************** */
/* ************************************************** */
int init(call_t *c, void *params) {
    struct entitydata *entitydata = malloc(sizeof(struct entitydata));
    param_t *param;

    /* default values */
    entitydata->frequency_w    = 2400;
    entitydata->distpower_lc_w = 30;
    entitydata->n_floors_w     = 2;
    entitydata->Lf_w           = -1;
    entitydata->cst_w          = 0;
    entitydata->frequency_z    = 868;
    entitydata->distpower_lc_z = 33;
    entitydata->n_floors_z     = 2;
    entitydata->Lf_z           = 19;
    entitydata->cst_z          = 0;

    /* get parameters */
    das_init_traverse(params);
    while ((param = (param_t *) das_traverse(params)) != NULL) {
        if (!strcmp(param->key, "frequency_MHz_w")) {
            if (get_param_double(param->value, &(entitydata->frequency_w))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "distpower_lc_w")) {
            if (get_param_double(param->value, &(entitydata->distpower_lc_w))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "n_floors_w")) {
            if (get_param_integer(param->value, &(entitydata->n_floors_w))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "Lf_w")) {
            if (get_param_double(param->value, &(entitydata->Lf_w))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "frequency_MHz_z")) {
            if (get_param_double(param->value, &(entitydata->frequency_z))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "distpower_lc_z")) {
            if (get_param_double(param->value, &(entitydata->distpower_lc_z))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "n_floors_z")) {
            if (get_param_integer(param->value, &(entitydata->n_floors_z))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "Lf_z")) {
            if (get_param_double(param->value, &(entitydata->Lf_z))) {
                goto error;
            }
        }
    }

    /* ----- protocol zigbee ----- */
    /* precomputed value for optimizing the simulation speedup */
    entitydata->cst_z = 20 * log10(entitydata->frequency_z) + entitydata->Lf_z - 28;

    /* ----- protocol wifi ----- */
    if (entitydata->Lf_w == -1){
       entitydata->Lf_w = 15 + 4 * (entitydata->n_floors_w - 1);
    }
    /* precomputed value for optimizing the simulation speedup */
    entitydata->cst_w = 20 * log10(entitydata->frequency_w) + entitydata->Lf_w - 28;


    set_entity_private_data(c, entitydata);
    return 0;

 error:
    free(entitydata);
    return -1;
}

int destroy(call_t *c) {
    free(get_entity_private_data(c));
    return 0;
}


/* ************************************************** */
/* ************************************************** */

double propagation(call_t *c, packet_t *packet, nodeid_t src, nodeid_t dst, double rxdBm) {
    struct entitydata *entitydata = get_entity_private_data(c);
    double dist = distance(get_node_position(src), get_node_position(dst));
    double L_dB;

    /*
     * Pr_dBm = Pt + Gt + Gr - L
     *
     * L_dB = 20 * log10(f) + N * log10(dist) + Lf(n) - 28
     * 
     * For the frequency 1.8-2.0 GHz
     * Lf(n) is the floor penetration loss factor (dB)
     * Lf(n) = 15 + 4(n-1); (n >= 1)
     *
     * Note: rxdBm = [Pt + Gt + Gr]_dBm
     *
     * ref1: recommendation ITU-R P.1238-1, propagation data and prediction methods for the planning of indoor radiocommunication systems, 1999.
     * ref2: http://en.wikipedia.org/wiki/ITU_Model_for_Indoor_Attenuation, 2009
     *
     */

     if(packet->type == 1){
        L_dB = entitydata->cst_z + entitydata->distpower_lc_z * log10(dist);
        return (rxdBm - L_dB);
     } else if(packet->type == 0){
        L_dB = entitydata->cst_w + entitydata->distpower_lc_w * log10(dist);
        return (rxdBm - L_dB);
     } else{
        free(entitydata);
        printf("[ERROR itu_indoor_multi_protocol] unexisting protocol\n");
        return -1;
     }
}

/* ************************************************** */
/* ************************************************** */
propagation_methods_t methods = {propagation};
