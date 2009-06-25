/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/* 
 * This is a driver for a Zytronic Touchscreen.
 * Note that is not the standard Zytronic (microsoft-DLL controllable) touchscreen;
 * It uses a micro-controller similar to the one of Elo or Mutouch touchscreens.
 * The configuration inside directfbrc is equal to Elo/Mutouch config.
 * All three touchscreens are conceptualised to use 4096x4096 virtual resolutions.
 *
 * This driver has been kindly provided by Jacques Luder.
 * Written by Eric Wajman - Jacques Luder.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <termios.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>

#include <linux/serial.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/system.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/memcpy.h>
#include <direct/thread.h>

#include <core/input_driver.h>

DFB_INPUT_DRIVER( zytronic )


// codes de retours :
  #define ZYT_ERR_NOT_START_OF_READ                   -1 // erreur si on demande un lecture de msg alors que l'on ne 
                                                         // reçoit pas ':' comme premier caractère (à priori ne devrait
                                                         // jamais arriver...?)
  #define ZYT_ERR_CANT_OPEN                           -2 // erreur si on n'arrive pas à ouvrir le "fichier" lié au device.
  #define ZYT_ACK_NACK                                -3 // prévient que le message lu est un ack/nack
                                                         // et non un vrai message
  #define ZYT_END_OF_CONF_FILE                        -4 // prévient que l'on a atteint la fin du fichier de configuration

// infos utiles :
  #define ZYT_PAQUET_SIZE                           128 // taille max d'un msg envoyé/reçu (pour la taille du tableau)
  #define ZYT_ENDOFMSG_CR                           13  // 1er caractère de fin de message reçu
  #define ZYT_ENDOFMSG_LF                           10  // 2ème caractère de fin de message reçu

// commandes protocoles (pas toutes utilisées dans ce driver) :
  #define ZYT_ENABLE_CONTROLLER                     201
  #define ZYT_DISABLE_CONTROLLER                    200

  #define ZYT_XY_TOUCH_MESSAGE_MODE                 210
  #define ZYT_INIT_FW_RESET                         72
  #define ZYT_FORCE_EQUALISATION                    69
  #define ZYT_SET_TOUCH_THRESHOLD                   100 // + valeur de Threshold voulue (entre 5 et 50)

  #define ZYT_REQ_FW_AND_ID                         86
  #define ZYT_REQ_NON_VOLATILE_SETTINGS             88
  #define ZYT_RESTORE_FACTORY_DEFAULT_SETTINGS      68
  #define ZYT_REQ_SHORT_STATUS_MESSAGE              71

  #define ZYT_REQ_SINGLE_FRAME_OF_RAW_SENSOR_DATA   82
  #define ZYT_ENABLE_CONTINUOUS_RAW_SENSOR_DATA     205
  #define ZYT_DISABLE_CONTINUOUS_RAW_SENSOR_DATA    204

  #define ZYT_SET_FRAME_AVERAGING                   220 // + nombre de frame pour "xy averaging" voulue (1 à 9)
  #define ZYT_SET_GLASS_THICKNESS                   230 // + valeur (ci dessous)
    #define ZYT_GLASS_THIN                          0
    #define ZYT_GLASS_MEDIUM                        1
    #define ZYT_GLASS_THICK                         2

  #define ZYT_ENABLE_CONTROLLER_ACK_NAK             203
  #define ZYT_DISABLE_CONTROLLER_ACK_NAK            202
  #define ZYT_STORE_DATA_BLOCK                      213 // + le code à enregistrer (voir la doc du protocole...)
  #define ZYT_RETRIEVE_DATA_BLOCK                   214 // + taile des infos à récupèrer (voir la doc du protocole...)

// paramètres de notre fichier de configuration :
  #define ZYT_CONF_FILE                             "/etc/zyposConf" // le nom de notre fichier de configuration
  #define ZYT_CONF_DIM_DALLE_X                      "dimDalleX"
  #define ZYT_CONF_DIM_DALLE_Y                      "dimDalleY"
  #define ZYT_CONF_SEUIL                            "seuil"
  #define ZYT_CONF_EPAISSEUR                        "epaisseur"
  #define ZYT_CONF_ATTENTE_MULTI_CLIC               "attenteMultiClic"
  #define ZYT_CONF_MODE                             "mode"
  #define ZYT_CONF_FRAME_AVERAGING                  "frameAveraging"
  #define ZYT_CONF_NB_CLIGN_TO_PRESS                "nbClignToPress"
  #define ZYT_CONF_DEBUG                            "debug"

// types d'action (pour ZytData->action):
  #define ZYT_ACTION_TOUCH                          0x01
  #define ZYT_ACTION_RELEASE                        0x00

// les masques :
  #define ZYT_MASK_ACTION                           0x40
  #define ZYT_MASK_SYNC_MSG                         0x80

typedef enum{
  NO_DRAG_DROP=0,
  CONTINUOUS=1,
} ZytMode;

typedef enum{
  NO_DEBUG=0,
  DEBUG=1,
} ZytDebug;

// structure du paramètrage
typedef struct {
  unsigned short dimDalleX;
  unsigned short dimDalleY;
  unsigned short seuil;
  unsigned short epaisseur;
  unsigned int attenteMultiClic;
  ZytMode mode;
  unsigned short frameRateAveraging;
  unsigned short nbClignToPress;
  ZytDebug debug;
} ZytConf_t;

/* structure de data du Zytronic */
typedef struct __ZytData__ {
  int fd;
  DirectThread *thread;
  CoreInputDevice *device;
  unsigned short x;
  unsigned short y;
  unsigned char action;
} ZytData;

/* Global Variables */
static unsigned char packet[ZYT_PAQUET_SIZE];
static struct termios options; // pour changer les configurations du port (avec cfmakeraw)
static struct termios saved_options;// pour sauvegarder l'ancienne configuration du port et la rétablir après
static ZytConf_t zytConf;

// fonction pour faire des mini-pauses en millisecondes : (utile? on garde)
static inline void __mdelay(unsigned int msec)
{
  struct timespec delay;

  delay.tv_sec = 0;
  delay.tv_nsec = msec * 1000000;
  nanosleep (&delay, NULL);
}

// procédure pour envoyer un paquet au controleur Zytronic, encapsulé d'une certaine manière par rapport
// au protocole (pour Zytronic il y n'a rien autour du code OP_CODE lui même)
static inline void ZytSendPacket(int file, unsigned char *msg, unsigned char len)
{
  write(file,msg,len);
}

// procédure pour lire un "paquet" de données venant du controleur (un message donc)
static int ZytReadMsg(int file, unsigned char *msg)
{
  int i=0;
  read(file,&msg[0],1);

  if(msg[0]==':'){ // si c'est un réponse à une commande :
    do{ // on lit tant qu'on trouve pas les 2 caractères de fin :
      i++; // on commence à lire à 1 (car le 0 est déjà lu)
      read(file,&msg[i],1);
    }while(msg[i]!=ZYT_ENDOFMSG_LF || msg[i-1]!=ZYT_ENDOFMSG_CR);
  }else if(msg[0]==0xC0 || msg[0]==0x80){ // si c'est un appui ou un relachement sur la dalle :
    // on lit les 4 caractères pour la position du touché :
    for(i=1;i<5;i++){
      read(file,&msg[i],1);
    }
  }else if(msg[0]==0x06 || msg[0]==0x15){ // si c'est un ACK/NACK on le dit
    if(zytConf.debug==DEBUG){
      D_INFO("ZYT, reception d'un ACK/NACK (6=ACK, 21=NACK): %d\n",msg[0]); //debug
    }
    return ZYT_ACK_NACK;
  }else { // sinon : ce n'est pas le début d'une lecture => on a perdu des donneés précédemment ...
    D_INFO("ZYT_ERR_NOT_START_OF_READ\n"); // ça ne devrait PAS arriver !
    return ZYT_ERR_NOT_START_OF_READ;
  }
  if(zytConf.debug==DEBUG){
    D_INFO("ZYT_READ_MSG : nb octets recus= %d\n",i); // debug
  }

  return 0;
}

#define WORD_ASSEMBLY(b1,b2) (((b2) << 7) | (b1))
// fonction qui permet de récupèrer les informations contenues dans un "message" en mode "touch XY mode" ***
static int ZytReadTouchMessage(ZytData* event){
  if( ZytReadMsg(event->fd,packet) != 0 ) {
    return 0; // on renvoit qu'on a pas pu lire correctement
  }

  // on récupère les infos x,y et appui ou relachement:
  if( (packet[0] & ZYT_MASK_ACTION) == 0 ){
    event->action = ZYT_ACTION_RELEASE;
  }else{
    event->action = ZYT_ACTION_TOUCH;
  }

  event->x = (float)(4096*WORD_ASSEMBLY(packet[1], packet[2])/zytConf.dimDalleX);
  event->y = (float)(4096*WORD_ASSEMBLY(packet[3], packet[4])/zytConf.dimDalleY);

 return 1; // on dit que on a effectivement bien lu
}

// procédure pour écrire un paramètre dans le fichier de configuration Zytronic :
static void ecrireConf(int f,char *sp, char *sv){
  char tmp[100];

  strcpy(tmp,":");
  strcat(tmp,sp);
  strcat(tmp,"=");
  strcat(tmp,sv);
  strcat(tmp,";\n");
  write(f,tmp,strlen(tmp));
}

// procédure pour créer le fichier de configuration avec les valeurs par défaut
static void createConfigFile(int *fdConf){
  char tmp[10];

  // création du fichier :
  *fdConf = open(ZYT_CONF_FILE,O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);

  // mise en place des valeurs par défaut :
  sprintf(tmp,"%d",zytConf.dimDalleX);
  ecrireConf(*fdConf,ZYT_CONF_DIM_DALLE_X,tmp);
  sprintf(tmp,"%d",zytConf.dimDalleY);
  ecrireConf(*fdConf,ZYT_CONF_DIM_DALLE_Y,tmp);
  sprintf(tmp,"%d",zytConf.seuil);
  ecrireConf(*fdConf,ZYT_CONF_SEUIL,tmp);
  sprintf(tmp,"%d",zytConf.epaisseur);
  ecrireConf(*fdConf,ZYT_CONF_EPAISSEUR,tmp);
  sprintf(tmp,"%d",zytConf.attenteMultiClic);
  ecrireConf(*fdConf,ZYT_CONF_ATTENTE_MULTI_CLIC,tmp);
  sprintf(tmp,"%d",zytConf.mode);
  ecrireConf(*fdConf,ZYT_CONF_MODE,tmp);
  sprintf(tmp,"%d",zytConf.frameRateAveraging);
  ecrireConf(*fdConf,ZYT_CONF_FRAME_AVERAGING,tmp);
  sprintf(tmp,"%d",zytConf.nbClignToPress);
  ecrireConf(*fdConf,ZYT_CONF_NB_CLIGN_TO_PRESS,tmp);
  sprintf(tmp,"%d",zytConf.debug);
  ecrireConf(*fdConf,ZYT_CONF_DEBUG,tmp);
  D_INFO("fsync renvoi la valeur: %d\n",fsync(*fdConf)); //pour s'assurer qu'on écrit bien dans le fichier
}

// procédure qui retourne le prochain paramètre lu dans le fichier de configuration
static int nextConf(int fdConf,char *param, char *res){
  char charActuel;
  int i,nb=1;

  // note : on suppose pour l'instant qu'aucune erreur de lecture n'intervient, ni de fichier mal formaté *** ...

  // on cherche le début d'un paramètre de configuration (ou la fin du fichier) :
  while(charActuel!=':' && nb==1){
    nb = read(fdConf,&charActuel,1);
  }
  if(nb==0){ // le cas échéant, on prévient que le fichier ne contient pas d'autres paramètres (fin du fichier)
    return ZYT_END_OF_CONF_FILE;
  }

  // on enregistre le nom du paramètre:
  i=0;
  read(fdConf,&charActuel,1);
  while(charActuel!='='){
    param[i]=charActuel;
    read(fdConf,&charActuel,1);
    i++;
  }
  param[i]='\0';


  // on enregistre la valeur de ce paramètre :
  i=0;
  read(fdConf,&charActuel,1);
  while(charActuel!=';'){
    res[i]=charActuel;
    read(fdConf,&charActuel,1);
    i++;
  }
  res[i]='\0';

  // on retourne le tout (en indiquant que tout s'est bien passé, on a pas atteint la fin du fichier)
  return 0;
}

// procédure pour "activer" le device, en l'occurence : mettre les paramètres de base qui nous intéressent nous :
static void ZytActivateDevice(int fd)
{
  int fdConf,val;
  char param[100],res[10];

  //paramètres par défaut : (également utilisés pour créer le fichier de configuration par la suite, s'il n'existe pas)
  zytConf.dimDalleX = 1024;
  zytConf.dimDalleY = 768;
  zytConf.seuil = 10;
  zytConf.epaisseur = 1;
  zytConf.attenteMultiClic = 300;
  // par défaut on ne garde que le premier appui d'une série : la "souris" ne bougera pas entre un appui et un relachement
  zytConf.mode=NO_DRAG_DROP;
  zytConf.frameRateAveraging=3;
  zytConf.nbClignToPress=5; // le nombre d'appuis successifs pour comprendre un appui après un clignotement
  zytConf.debug=NO_DEBUG;

  // récupération des paramètres choisis, et création du fichier si besoin :
  fdConf = open(ZYT_CONF_FILE,O_RDWR);
  if(fdConf==-1){
    D_INFO("ZYT, le fichier %s de configuration de la dalle zytronic n'existe pas, creation en cours...\n",ZYT_CONF_FILE);
    createConfigFile(&fdConf);
    D_INFO("ZYT, ...creation de %s finie.\n",ZYT_CONF_FILE);
  }else{
    D_INFO("ZYT, le fichier %s de configuration de la dalle zytronic existe.\n",ZYT_CONF_FILE);
  }

  // puis, on charge les valeurs de configuration, en gardant les valeurs par défaut pour les paramètres non précisés :
  lseek(fdConf,0,SEEK_SET); // on retourne au début du fichier (au cas où on vient de le créé)
  while(nextConf(fdConf,param,res)!=ZYT_END_OF_CONF_FILE){
    val = atoi(res);
    D_INFO("parametre : %s = %d\n",param,val); //debug
    if(strcmp(param,ZYT_CONF_DIM_DALLE_X)==0){
      zytConf.dimDalleX=val;
    }else if(strcmp(param,ZYT_CONF_DIM_DALLE_Y)==0){
      zytConf.dimDalleY=val;
    }else if(strcmp(param,ZYT_CONF_SEUIL)==0){
      zytConf.seuil=val;
    }else if(strcmp(param,ZYT_CONF_EPAISSEUR)==0){
      zytConf.epaisseur=val;
    }else if(strcmp(param,ZYT_CONF_ATTENTE_MULTI_CLIC)==0){
      zytConf.attenteMultiClic=val*1000; //on passe les millisecondes en microsecondes
    }else if(strcmp(param,ZYT_CONF_MODE)==0){
      zytConf.mode=val;
    }else if(strcmp(param,ZYT_CONF_FRAME_AVERAGING)==0){
      zytConf.frameRateAveraging=val;
    }else if(strcmp(param,ZYT_CONF_NB_CLIGN_TO_PRESS)==0){
      zytConf.nbClignToPress=val;
    }else if(strcmp(param,ZYT_CONF_DEBUG)==0){
      zytConf.debug=val;
    }else {
      D_INFO("ZYT, parametre non reconnu : %s\n",param);
      D_INFO("ZYT, veuillez verifier le  fichier de configuration %s!\n",ZYT_CONF_FILE);
    }
  }
  close(fdConf);

  // envoi des paramètres du controleur pour utilisation normale :
  //packet[0]=ZYT_INIT_FW_RESET; // permet de "mieux" initialiser le controleur, utile? (long : ~5sec)
  //ZytSendPacket(fd,packet,1); // attention, commande inutilisable telle qu'elle, car elle coupe la liaison
     // et donc empêche les commandes suivantes...
     // (ici en l'occurence tout le paramètrage qui suit...) !!!
  packet[0]=ZYT_RESTORE_FACTORY_DEFAULT_SETTINGS;
  ZytSendPacket(fd,packet,1);
  packet[0]=ZYT_SET_FRAME_AVERAGING + zytConf.frameRateAveraging;
  ZytSendPacket(fd,packet,1);
  packet[0]=ZYT_DISABLE_CONTINUOUS_RAW_SENSOR_DATA;
  ZytSendPacket(fd,packet,1);
  packet[0]=ZYT_SET_TOUCH_THRESHOLD + zytConf.seuil;
  ZytSendPacket(fd,packet,1);
  packet[0]=ZYT_SET_GLASS_THICKNESS + zytConf.epaisseur;
  ZytSendPacket(fd,packet,1);
  packet[0]=ZYT_ENABLE_CONTROLLER;
  ZytSendPacket(fd,packet,1);
  packet[0]=ZYT_FORCE_EQUALISATION;
  ZytSendPacket(fd,packet,1);
  packet[0]=ZYT_XY_TOUCH_MESSAGE_MODE;
  ZytSendPacket(fd,packet,1);
}

// fonction pour "ouvrir" le périphérique
// (càd: ouvrir le fichier spécial qui permet de communiquer avec le controleur)
static int ZytOpenDevice(char *device)
{
     int fd;
     fd = open (device, O_RDWR | O_NOCTTY); // pourquoi 0_NOCTTY ? ***
     if ( fd == -1 ) {
          return ZYT_ERR_CANT_OPEN;
     }

     // on récupère l'actuelle configuration du port :
     tcgetattr(fd,&options);
     tcgetattr(fd,&saved_options);

     // on passe en 96000 bauds :
     cfsetospeed(&options,B9600);
     cfsetispeed(&options,B9600);

     // on passe en mode de fonctionnement "pur" pour pouvoir utiliser correctement le port série :
     cfmakeraw(&options);
     tcsetattr(fd,TCSANOW,&options);

     return fd;
}

// le thread qui sert à recevoir des données en continue :
static void *ZytronicEventThread(DirectThread *thread, void *driver_data)
{
     ZytData *data = (ZytData *) driver_data;
     int lastAction = ZYT_ACTION_RELEASE;
     struct timeval unT;
     unsigned int lastT,newT;
     unsigned short nbClignot=0;
     lastT = 0;
     DFBInputEvent evt;

     /* Read data */
     while (1) {
          if (!ZytReadTouchMessage (data)){ // si jamais il y a eut mauvaise lecture (pas normal)
               continue; // on ignore le mesage mal lu
          }
          // en mode sans drag&drop, si l'action actuelle est la même que la précédente, on l'ignore:
          if (zytConf.mode==NO_DRAG_DROP && lastAction == data->action){
               nbClignot=0; // et on dit que ça clignote pas (car c'est un appui long, pas un clignotement)
               continue;		// permet de ne garder que le premier appui, et le relachement :
          }
          gettimeofday(&unT,NULL);
          newT = unT.tv_sec*1000000 + unT.tv_usec;
          if(zytConf.debug==DEBUG){
               D_INFO("newT=%u\n",newT);
               D_INFO("lastT=%u\n",lastT);
               D_INFO("lastT+attente=%u\n",lastT+zytConf.attenteMultiClic);
          }

          // si on "appui" trop vite, sans faire un appui continu, on réenregistre la dernière action,... :
          if(data->action==ZYT_ACTION_TOUCH && nbClignot < zytConf.nbClignToPress && \
          (lastT + zytConf.attenteMultiClic) > newT) {
               nbClignot++;// ..on compte combien de fois de suite on essai d'appuyer (pour voir si c'est la fin
                           // d'un clignotement justement parce qu'on a rapprocher le doigt suffisemment) ..
               gettimeofday(&unT,NULL);
               lastT = unT.tv_sec*1000000 + unT.tv_usec;
               continue; // ..et on ignore cet appui (permet d'éviter le phénomène de clignotement..)
          }
          nbClignot=0; // on remet le compteur à zéro, puisque c'est ici un appui réel
          direct_thread_testcancel (thread); // si cette ligne fait bien ce que je pense (regarder si on n'a pas
            // demandé la fin du thread en cours) pourquoi est-elle là, et pas avant le "if" ? Car si le controlleur
            // n'envoi plus aucune information pendant un moment, le driver ne peut pas détecter de "cancel"
            // pendant ce laps de temps .. ? ***

          // Dispatch axis
          evt.type    = DIET_AXISMOTION;
          evt.flags   = DIEF_AXISABS;
          evt.axis    = DIAI_X;
          evt.axisabs = data->x;
          dfb_input_dispatch (data->device, &evt);

          evt.type    = DIET_AXISMOTION;
          evt.flags   = DIEF_AXISABS;
          evt.axis    = DIAI_Y;
          evt.axisabs = data->y;
          dfb_input_dispatch (data->device, &evt);

          // Dispatch touch event
          switch (data->action) {
               case ZYT_ACTION_TOUCH:
                    evt.type = DIET_BUTTONPRESS;
                    break;
               case ZYT_ACTION_RELEASE:
                    evt.type = DIET_BUTTONRELEASE;
                    break;
          }


          evt.flags  = DIEF_NONE;
          evt.button = DIBI_LEFT;
          dfb_input_dispatch (data->device, &evt);

          lastAction = data->action; // on enregistre l'évènement
          gettimeofday(&unT,NULL); // on enregistre quand s'est produit l'évènement
          lastT = unT.tv_sec*1000000 + unT.tv_usec;

          if(zytConf.debug==DEBUG){
               D_INFO("Zytronic TOUCH : x=%d y=%d action=%d\n", data->x,data->y,data->action);
          }

          direct_thread_testcancel (thread);
     }

     return NULL;
}


/* exported symbols */

// procédure pour dire à directFB si c'est bien ce driver qu'il faut charger :
static int driver_get_available( void )
{
     int fd;

     /* we only try to open the device if it has been actually configured */
     if( !dfb_config->zytronic_device )
          return 0;

     fd = ZytOpenDevice(dfb_config->zytronic_device );
     D_INFO( "Zytronic:driver_get_available %s fd %d\n", dfb_config->zytronic_device,fd );

     if (fd < 0){
          D_INFO( "The Zytronic driver cannot be loaded from %s\n", dfb_config->zytronic_device );
          return 0;
     }

     close(fd);
     return 1;
}

// donne la description du driver :
static void driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf(info->name, DFB_INPUT_DRIVER_INFO_NAME_LENGTH,
               "Zypos" );
     snprintf(info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "Zytronic" );

     info->version.major = 0;
     info->version.minor = 5;
}

// "ouvre" le device et commnce à le préparer :
static DFBResult driver_open_device(CoreInputDevice *device,
          unsigned int number,
          InputDeviceInfo *info,
          void **driver_data)
{
     int fd;
     ZytData *data;

     /* open device */
     fd = ZytOpenDevice (dfb_config->zytronic_device);
     D_INFO("ZYT, driver_open_device %s fd %d\n", dfb_config->zytronic_device,fd);

     if (fd < 0) {
          return DFB_INIT;
     }

     ZytActivateDevice(fd); //on configure le controleur pour fonctionner en mode normal

     data = D_CALLOC (1, sizeof(ZytData));

     data->fd     = fd;
     data->device = device;

     /* fill device info structure */
     snprintf(info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH,
               "Zypos");
     snprintf(info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH,
               "Zytronic");

     info->prefered_id     = DIDID_MOUSE;
     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis   = DIAI_Y;
     info->desc.max_button = DIBI_LEFT;

     /* start input thread */
     data->thread = direct_thread_create (DTT_INPUT, ZytronicEventThread, data, "Zytronic Input");

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

/*
 * Fetch one entry from the device's keymap if supported.
 */
static DFBResult driver_get_keymap_entry(CoreInputDevice *device,
          void        *driver_data,
          DFBInputDeviceKeymapEntry *entry)
{
     return DFB_UNSUPPORTED;
}

// "fermeture" du driver ***
static void driver_close_device(void *driver_data)
{
     ZytData *data = (ZytData *)driver_data;

     /* stop input thread */
     direct_thread_cancel (data->thread);
     direct_thread_join (data->thread);
     direct_thread_destroy (data->thread);

     /* close device */
     tcsetattr(data->fd,TCSANOW,&saved_options); // remise en l'état de l'ancienne configuration du port
     close (data->fd);

     /* free private data */
     D_FREE (data);
}
