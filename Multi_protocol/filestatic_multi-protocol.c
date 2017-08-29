/**
 *  \file   filestatic_multi-protocol.c
 *  \brief  Static routing extracted from file
 *  \author Doreid Ammar
 *  \date   2009
 **/

#include <stdio.h>
#include <include/modelutils.h>

/* ************************************************** */
/* ************************************************** */
model_t model =  {
    "Static routing multi-protocol",
    "Doreid AMMAR",
    "0.1",
    MODELTYPE_ROUTING, 
    {NULL, 0}
};

/* ------- Bundle Architecture --- xml file --- (multi-protocol node) -------------------------------
                                                  -->  mac-wifi  -->  radio-wifi  -->  Antenna-wifi
Application layer --> filestatic_multi-protocol --
                                                  --> mac-zigbee --> radio-zigbee --> Antenna-zigbee
-------------------------------------------------------------------------------------------------- */

#define wifi 1			/* mono-protocol node (protocol  802.11g) */
#define zigbee 2		/* mono-protocol node (protocol 802.15.4) */
#define protocol 0		/* multi-protocol node */

/* ************************************************** */
/* ************************************************** */
struct route {
    nodeid_t dst;
    nodeid_t n_hop;		/* next hop node id */
    nodeid_t p_n_hop;		/* the protocol type for the next hop node */
};

struct routing_header {
    nodeid_t dst;
    nodeid_t src;
};

struct entitydata {
    FILE *file;
    int data_size_wifi;
    int data_size_zigbee;
};

struct nodedata {
    void *routes;
    int protocol_type;		/* the protocol type for each packet send */
    int overhead[2];
    int node_protocol;		/* the initial node protocol */
    int protocol_priority;	/* specified a protocol to use in case of two multi-protocol nodes */
    int i;			/* chose the overhead in transmission */
    int j;			/* chose the overhead in reception */
    int dst;			/* the destination id from the Application layer (using ioctl)*/
};

/* ************************************************** */
/* ************************************************** */
unsigned long route_hash(void *key) { 
    return (unsigned long) key;
}

int route_equal(void *key0, void *key1) { 
    return (int) (key0 == key1);
}

/* ************************************************** */
/* ************************************************** */
int init(call_t *c, void *params) {
    struct entitydata *entitydata = malloc(sizeof(struct entitydata));
    param_t *param;
    char *filepath = NULL;

    /* default values */
    filepath       = "routing.data";
    entitydata->data_size_wifi = 1000;
    entitydata->data_size_zigbee = 1000;

    /* get parameters */
    das_init_traverse(params);
    while ((param = (param_t *) das_traverse(params)) != NULL) {
        if (!strcmp(param->key, "file")) {
            filepath = param->value;
        }
        if (!strcmp(param->key, "data_size_wifi")) {
            if (get_param_integer(param->value, &(entitydata->data_size_wifi))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "data_size_zigbee")) {
            if (get_param_integer(param->value, &(entitydata->data_size_zigbee))) {
                goto error;
            }
        }
    }
  
    /* open file */
    if ((entitydata->file = fopen(filepath, "r")) == NULL) {
        fprintf(stderr, "filestatic: can not open file %s in init()\n", filepath);
        goto error;
    }
    
    set_entity_private_data(c, entitydata);
    return 0;

 error:
    free(entitydata);
    return -1;
}

int destroy(call_t *c) {
    struct entitydata *entitydata = get_entity_private_data(c);

    if (entitydata->file != NULL) {
        fclose(entitydata->file);
    }

    free(entitydata);
    return 0;
}

/* ************************************************** */
/* ************************************************** */
int setnode(call_t *c, void *params) {
    struct entitydata *entitydata = get_entity_private_data(c);
    struct nodedata *nodedata = malloc(sizeof(struct nodedata));
    param_t *param;

    /* default values */
    nodedata->node_protocol	= protocol;
    nodedata->protocol_priority	= wifi;

    /* get params */
    das_init_traverse(params);
    while ((param = (param_t *) das_traverse(params)) != NULL) {
        if (!strcmp(param->key, "node_protocol")) {
            if (get_param_integer(param->value, &(nodedata->node_protocol))) {
                goto error;
            }
        }
        if (!strcmp(param->key, "protocol_priority")) {
            if (get_param_integer(param->value, &(nodedata->protocol_priority))) {
                goto error;
            }
        }
    }

    char str[128];
    int id, dst, n_hop, p_n_hop;
    
    /* extract routing table from file */
    nodedata->routes = hadas_create(route_hash, route_equal);
    
    /* extract routing table from file */
    fseek(entitydata->file, 0L, SEEK_SET);
    while (fgets(str, 128, entitydata->file) != NULL) {
        if (sscanf(str, "%d %d %d %d\n",  &id, &dst, &n_hop, &p_n_hop) != 4) {
            fprintf(stderr, "filestatic: unable to read route in setnode()\n");
            goto error;
        }
        
        if (id == c->node) {
            struct route *route = (struct route *) malloc(sizeof(struct route));

	    route->dst	   = dst;
            route->n_hop   = n_hop;
	    route->p_n_hop = p_n_hop;
            hadas_insert(nodedata->routes, (void *) ((unsigned long) (route->dst)), (void *) route);
	}
    }  
    nodedata->overhead[1] = nodedata->overhead[0] = -1; 
    set_node_private_data(c, nodedata);
    return 0;

 error:
    free(entitydata);
    return -1;
}

int unsetnode(call_t *c) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct route *route;
    int i = get_node_count();
    while (i--) {
        if ((route = (struct route *) hadas_get(nodedata->routes, (void *) ((unsigned long) i))) != NULL) {
            free(route);
        }
    }
    hadas_destroy(nodedata->routes);    
    free(nodedata);
    return 0;
}


/* ************************************************** */
/* ************************************************** */
int bootstrap(call_t *c) {
    struct nodedata *nodedata = get_node_private_data(c);

    /* get overhead */

      /* mono-protocol node (wifi || zigbee) */
      if (nodedata->node_protocol == wifi || nodedata->node_protocol == zigbee){
         call_t c0 = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};
         nodedata->overhead[0] = GET_HEADER_SIZE(&c0);
         return 0;
      }
      /* multi-protocol node (wifi && zigbee) */
      else if (nodedata->node_protocol == protocol){
         int i=1;
         while (i>=0){
            call_t c0 = {get_entity_bindings_down(c)->elts[i], c->node, c->entity};
            nodedata->overhead[i] = GET_HEADER_SIZE(&c0);
            i--;
         }
         return 0;
      }else {
         free(nodedata);
         printf("[ERROR Multi-Protocol] unexisting node protocol\n");
         return -1;
      }
}

int ioctl(call_t *c, int option, void *in, void **out) {
/* ---- Application layer - Call IOCTL ------ copy to the function setnode (Application layer) -----
       int j = get_entity_links_down_nbr(c);
       entityid_t *down = get_entity_links_down(c);
       while (j--) {
	   call_t c0 = {down[j], c->node, c->entity};
	   if (get_entity_type(&c0) == MODELTYPE_ROUTING){
             IOCTL(&c0, 1, (void *) nodedata->destination, NULL);
	     break;
           }
       } 
-------------------------------------------- copy to the function setnode (Application layer) ---- */
    struct nodedata *nodedata = get_node_private_data(c);
    int A = (void*) in;
    switch (option) {
	case 1 : nodedata->dst = A;
    }
    return 0;
}


/* ************************************************** */
/* ************************************************** */
int set_header(call_t *c, packet_t *packet, destination_t *dst) {
    int i;
    struct nodedata *nodedata = get_node_private_data(c);
    struct route *route = hadas_get(nodedata->routes, (void *) ((unsigned long) (dst->id)));
    if ((nodedata->node_protocol == wifi && route->p_n_hop == zigbee) || (nodedata->node_protocol == zigbee && route->p_n_hop == wifi)){
      nodedata->protocol_type = -1;
    }else if (nodedata->node_protocol == wifi || route->p_n_hop == wifi){
      nodedata->protocol_type = wifi;
    }else if (nodedata->node_protocol == zigbee || route->p_n_hop == zigbee){
      if(nodedata->node_protocol == zigbee){
	i=0;
      }
      if(route->p_n_hop == zigbee){
	i=1;
      }
      nodedata->protocol_type = zigbee;
    }else{
      switch (nodedata->protocol_priority){
	case 1:
	   nodedata->protocol_type = wifi;
	   break;
	case 2:
	   i=1;
	   nodedata->protocol_type = zigbee;
	   break;
	default:
	   nodedata->protocol_type = -1;
      }
    }
    
    if(nodedata->protocol_type == wifi){
      nodedata->i = 0;
      struct routing_header *header = (struct routing_header *) (packet->data + nodedata->overhead[0]);
      call_t c0 = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};
      destination_t n_hop;

      if (route == NULL) {
          return -1;
      }

      header->dst = dst->id;
      header->src = c->node;
      n_hop.id = route->n_hop;
      return SET_HEADER(&c0, packet, &n_hop);
    }else if(nodedata->protocol_type == zigbee){
      nodedata->i = i;
      struct routing_header *header = (struct routing_header *) (packet->data + nodedata->overhead[nodedata->i]);
      call_t c0 = {get_entity_bindings_down(c)->elts[nodedata->i], c->node, c->entity};
      destination_t n_hop;

      if (route == NULL) {
        return -1;
      }

      header->dst = dst->id;
      header->src = c->node;
      n_hop.id = route->n_hop;
      return SET_HEADER(&c0, packet, &n_hop);
    }else{
      packet_dealloc(packet);
      printf("[ERROR Multi-Protocols] node %d can not send packet to node %d\n", c->node,route->n_hop);
      return -1;
    }
}

int get_header_size(call_t *c) {
    struct nodedata *nodedata = get_node_private_data(c);

    /* get overhead */

      /* mono-protocol node (wifi || zigbee) */
      if (nodedata->node_protocol == wifi || nodedata->node_protocol == zigbee){
          if (nodedata->overhead[0] == -1) {
              call_t c0 = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};
              nodedata->overhead[0] = GET_HEADER_SIZE(&c0);
          }
          return nodedata->overhead[0] + sizeof(struct routing_header);
      }
      /* multi-protocol node (wifi && zigbee) */
      else if (nodedata->node_protocol == protocol){
         int i=1;
         while (i>=0){
            if (nodedata->overhead[i] == -1){
               call_t c0 = {get_entity_bindings_down(c)->elts[i], c->node, c->entity};
               nodedata->overhead[i] = GET_HEADER_SIZE(&c0);
            }
            i--;
         }
         struct route *route = hadas_get(nodedata->routes, (void *) ((unsigned long) (nodedata->dst)));
         if (route->p_n_hop == wifi){
            return nodedata->overhead[0] + sizeof(struct routing_header);
         } else if (route->p_n_hop == zigbee){
            return nodedata->overhead[1] + sizeof(struct routing_header);
         } else {
            switch (nodedata->protocol_priority){
	       case 1:
                  return nodedata->overhead[0] + sizeof(struct routing_header);
	       case 2:     
                  return nodedata->overhead[1] + sizeof(struct routing_header);
	       default:
                  free(nodedata);
                  printf("[ERROR Multi-Protocol] unexisting node protocol priority\n");
                  return -1;
             }
         }
      }else {
         free(nodedata);
         printf("[ERROR Multi-Protocol] unexisting node protocol\n");
         return -1;
      }
}

/* ************************************************** */
/* ************************************************** */
void tx(call_t *c, packet_t *packet) {
   struct nodedata *nodedata = get_node_private_data(c);
   array_t *down = get_entity_bindings_down(c);
   call_t c0 = {down->elts[nodedata->i], c->node, c->entity};
   TX(&c0, packet);  
}

/* ************************************************** */
/* ************************************************** */
void forward(call_t *c, packet_t *packet) { 
    int i;
    struct nodedata *nodedata = get_node_private_data(c);
    struct entitydata *entitydata = get_entity_private_data(c);
    struct routing_header *header = (struct routing_header *) (packet->data + nodedata->overhead[nodedata->j]);
    struct route *route = hadas_get(nodedata->routes, (void *) ((unsigned long) (header->dst)));

    if (route == NULL) {
        packet_dealloc(packet);
        return;
    }

    if ((nodedata->node_protocol == wifi && route->p_n_hop == zigbee) || (nodedata->node_protocol == zigbee && route->p_n_hop == wifi)){
      nodedata->protocol_type = -1;
    }else if (nodedata->node_protocol == wifi || route->p_n_hop == wifi){
      nodedata->protocol_type = wifi;
    }else if (nodedata->node_protocol == zigbee || route->p_n_hop == zigbee){
      if(nodedata->node_protocol == zigbee){
	i=0;
      }
      if(route->p_n_hop == zigbee){
	i=1;
      }
      nodedata->protocol_type = zigbee;
    }else{
      switch (nodedata->protocol_priority){
	case 1:
	   nodedata->protocol_type = wifi;
	   break;
	case 2:
	   i=1;
	   nodedata->protocol_type = zigbee;
	   break;
	default:
	   nodedata->protocol_type = -1;
      }
    }

    /* Forward packet using the protocol wifi */
    if(nodedata->protocol_type == wifi){
       call_t c0 = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};

       /* packet zigbee === FORWARD ===> packet wifi */
       if (packet->type == 1){
         packet_t *packet0 = packet_alloc(c, nodedata->overhead[0] - nodedata->overhead[nodedata->j] + entitydata->data_size_wifi - entitydata->data_size_zigbee + packet->size);
         struct routing_header *header0 = (struct routing_header *) (packet0->data + nodedata->overhead[0]);
         void *p_copy;
         p_copy = header;
         void *p_paste;
         p_paste = header0;
         memcpy(p_paste, p_copy, packet->size -nodedata->overhead[nodedata->j] - entitydata->data_size_zigbee);
         packet0 ->type = 0;

         packet_dealloc(packet);
         destination_t destination0 = {route->n_hop, {-1, -1, -1}};
         if (SET_HEADER(&c0, packet0, (void *) &destination0) == -1) {
            packet_dealloc(packet0);
            return;
         }
         /* Fragmentation --- packet zigbee --- */
         int nb_packet_wifi = entitydata->data_size_zigbee / entitydata->data_size_wifi;
         int reste = entitydata->data_size_zigbee % entitydata->data_size_wifi;
         if (reste == 0) {
           nb_packet_wifi--;
         }
         TX(&c0, packet0);
         while (nb_packet_wifi > 0) {
            packet_t *packet_frag;
            packet_frag = packet_clone(packet0);
            nb_packet_wifi--;
            TX(&c0, packet_frag);
         }
       }/* packet wifi === FORWARD ===> packet wifi */
        else{
         destination_t destination;
         destination.id = route->n_hop;
         if (SET_HEADER(&c0, packet, (void *) &destination) == -1) {
           packet_dealloc(packet);
           return;
         }
         TX(&c0, packet);         
       } 
    }/* Forward packet using the protocol zigbee */
     else if(nodedata->protocol_type == zigbee){
         call_t c0 = {get_entity_bindings_down(c)->elts[i], c->node, c->entity};

         /* packet wifi === FORWARD ===> packet zigbee */
         if (packet->type == 0){
	 packet_t *packet0 = packet_alloc(c, nodedata->overhead[1] - nodedata->overhead[nodedata->j] + entitydata->data_size_zigbee - entitydata->data_size_wifi + packet->size);
         struct routing_header *header0 = (struct routing_header *) (packet0->data + nodedata->overhead[1]);
         void *p_copy;
         p_copy = header;
         void *p_paste;
         p_paste = header0;
         memcpy(p_paste, p_copy, packet->size - nodedata->overhead[nodedata->j] - entitydata->data_size_wifi);
         packet0 ->type = 1;

         packet_dealloc(packet);
         destination_t destination0 = {route->n_hop, {-1, -1, -1}};
         if (SET_HEADER(&c0, packet0, (void *) &destination0) == -1) {
            packet_dealloc(packet0);
            return;
         }
         /* Fragmentation --- packet wifi ---*/
         int nb_packet_zigbee = (entitydata->data_size_wifi / entitydata->data_size_zigbee);
         int reste = entitydata->data_size_wifi % entitydata->data_size_zigbee;
         if (reste == 0) {
           nb_packet_zigbee--;
         }
         TX(&c0, packet0);
         while (nb_packet_zigbee > 0) {
            packet_t *packet_frag;
            packet_frag = packet_clone(packet0);
            nb_packet_zigbee--;
            TX(&c0, packet_frag);
         }
       }/* packet zigbee === FORWARD ===> packet zigbee */
        else{
         destination_t destination;
         destination.id = route->n_hop;
         if (SET_HEADER(&c0, packet, (void *) &destination) == -1) {
           packet_dealloc(packet);
           return;
         }
         TX(&c0, packet);         
       }
    }else{
      packet_dealloc(packet);
      printf("[ERROR Multi-Protocols] node %d can not send packet to node %d\n", c->node,route->n_hop);
      return;
    }
}

void rx(call_t *c, packet_t *packet) {   
    struct nodedata *nodedata = get_node_private_data(c);
    array_t *up = get_entity_bindings_up(c);
    int i = up->size;
    nodedata->j = 0;
    if (nodedata->node_protocol == protocol){
       switch(packet->type){
          case 0: nodedata->j = 0;
                  break; // if (packet->type == 0) => packet wifi
          case 1: nodedata->j = 1;
                  break; // if (packet->type == 1) => packet zigbee
          default: nodedata->j = -1;
       }
       if (nodedata->j == -1){
          return;
       }        
    }   
    struct routing_header *header = (struct routing_header *) (packet->data + nodedata->overhead[nodedata->j]);
    if ((header->dst != c->node) ) {
        forward(c, packet);
        return;
    }
    while (i--) {
        call_t c_up = {up->elts[i], c->node, c->entity};
        packet_t *packet_up;
        
        if (i > 0) {
            packet_up = packet_clone(packet);         
        } else {
            packet_up = packet;
        }
        RX(&c_up, packet_up);
    }
}


/* ************************************************** */
/* ************************************************** */
routing_methods_t methods = {rx, 
                             tx, 
                             set_header, 
                             get_header_size};
