/*
  * Copyright (C) 2011  Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
  * Author: Marco Randazzo
  * email: marco.randazzo@iit.it
  * Permission is granted to copy, distribute, and/or modify this program
  * under the terms of the GNU General Public License, version 2 or any
  * later version published by the Free Software Foundation.
  *
  * A copy of the license can be found at
  * http://www.robotcub.org/icub/license/gpl.txt
  *
  * This program is distributed in the hope that it will be useful, but
  * WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
  * Public License for more details
*/

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/dev/all.h>
#include <iCub/ctrl/math.h>
#include <iCub/skinDynLib/skinContact.h>
#include <iCub/skinDynLib/skinContactList.h>
#include <string>

#include "robot_interfaces.h"

using namespace iCub::skinDynLib;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::dev;
using namespace std;

#define POS  0
#define TRQ  1

//                robot->icmd[rd->id]->setPositionMode(0);


#define jjj 0
class CtrlThread: public yarp::os::PeriodicThread
{
    public:
    robot_interfaces *robot;
    bool   left_arm_master;
    double encoders_master [16];
    double encoders_slave  [16];
    bool   autoconnect;
    bool   stiff;
    Stamp  info;
    
    BufferedPort<iCub::skinDynLib::skinContactList> *port_skin_contacts;
    BufferedPort<Vector> *port_left_arm;
    BufferedPort<Vector> *port_right_arm;


    CtrlThread(unsigned int _period, ResourceFinder &_rf) :
               PeriodicThread((double)_period/1000.0)
    {
        autoconnect = false;
        robot=0;
        left_arm_master=false;
        port_skin_contacts=0;
        stiff = _rf.check("stiff");
    };

    virtual bool threadInit()
    {
        robot=new robot_interfaces(LEFT_ARM, RIGHT_ARM);
        robot->init();

        port_skin_contacts = new BufferedPort<skinContactList>;
        port_left_arm = new BufferedPort<Vector>;
        port_right_arm = new BufferedPort<Vector>;
        port_skin_contacts->open("/demoForceImitation/skin_contacts:i");
        port_left_arm->open("/demoForceImitation/left_arm:o");
        port_right_arm->open("/demoForceImitation/right_arm:o");

        if (autoconnect)
        {
            Network::connect("/skinManager/skin_events:o","/demoForceImitation/skin_contacs:i","tcp",false);
        }
        
        robot->iimp[LEFT_ARM]->setImpedance(0,0.2,0.02);
        robot->iimp[LEFT_ARM]->setImpedance(1,0.2,0.02);
        robot->iimp[LEFT_ARM]->setImpedance(2,0.2,0.02);
        robot->iimp[LEFT_ARM]->setImpedance(3,0.2,0.02);
        robot->iimp[LEFT_ARM]->setImpedance(4,0.1,0.00);

        robot->iimp[RIGHT_ARM]->setImpedance(0,0.2,0.02);
        robot->iimp[RIGHT_ARM]->setImpedance(1,0.2,0.02);
        robot->iimp[RIGHT_ARM]->setImpedance(2,0.2,0.02);
        robot->iimp[RIGHT_ARM]->setImpedance(3,0.2,0.02);
        robot->iimp[RIGHT_ARM]->setImpedance(4,0.1,0.00);

        yInfo("Going to home position...");
        for (int i=0; i<5; i++)
        {
            double tmp_pos=0.0;
            robot->ienc[RIGHT_ARM]->getEncoder(i,&tmp_pos);
            robot->icmd[LEFT_ARM]->setControlMode(i, VOCAB_CM_POSITION);
            robot->icmd[RIGHT_ARM]->setControlMode(i, VOCAB_CM_POSITION);
            robot->iint[LEFT_ARM]->setInteractionMode(i,VOCAB_IM_STIFF);
            robot->iint[RIGHT_ARM]->setInteractionMode(i,VOCAB_IM_STIFF);
            robot->ipos[LEFT_ARM]->setRefSpeed(i,10);
            robot->ipos[LEFT_ARM]->positionMove(i,tmp_pos);
        }
        double timeout = 0;
        do
        {
            int ok=0;
            for (int i=0; i<5; i++)
            {
                double tmp_pos_l=0;
                double tmp_pos_r=0;
                robot->ienc[LEFT_ARM]->getEncoder(i,&tmp_pos_l);
                robot->ienc[RIGHT_ARM]->getEncoder(i,&tmp_pos_r);
                if (fabs(tmp_pos_l-tmp_pos_r)<1.0) ok++;
            }
            if (ok==5) break;
            yarp::os::Time::delay(1.0);
            timeout++;
        }
        while (timeout < 10); //10 seconds
        if (timeout >=10)
        {
            yError("Unable to reach safe initial position! Closing module");
            return false;
        }

        change_master();

        yInfo("Position tracking started");
        return true;
    }
    virtual void run()
    {    
        int  i_touching_left=0;
        int  i_touching_right=0;
        int  i_touching_diff=0;
        info.update();
        
        skinContactList *skinContacts  = port_skin_contacts->read(false);
        if(skinContacts)
        {
            for(skinContactList::iterator it=skinContacts->begin(); it!=skinContacts->end(); it++){
                if(it->getBodyPart() == LEFT_ARM)
                    i_touching_left += it->getActiveTaxels();
                else if(it->getBodyPart() == RIGHT_ARM)
                    i_touching_right += it->getActiveTaxels();
            }
        }
        i_touching_diff=i_touching_left-i_touching_right;

        if (abs(i_touching_diff)<5)
        {
            yInfo("nothing!\n");
        }
        else
        if (i_touching_left>i_touching_right)
        {
            yInfo("Touching left arm! \n");
            if (!left_arm_master) change_master();
        }
        else
        if (i_touching_right>i_touching_left)
        {
            yInfo("Touching right arm! \n");
            if (left_arm_master) change_master();
        }

        if (left_arm_master)
        {
            robot->ienc[LEFT_ARM] ->getEncoders(encoders_master);
            robot->ienc[RIGHT_ARM]->getEncoders(encoders_slave);
            if (port_left_arm->getOutputCount()>0)
            {
                port_left_arm->prepare()= Vector(16,encoders_master);
                port_left_arm->setEnvelope(info);
                port_left_arm->write();
            }
            if (port_right_arm->getOutputCount()>0)
            {
                port_right_arm->prepare()= Vector(16,encoders_slave);
                port_right_arm->setEnvelope(info);
                port_right_arm->write();
            }            
            
            for (int i=jjj; i<5; i++)
            {
                robot->idir[RIGHT_ARM]->setPosition(i,encoders_master[i]);
            }
        }
        else
        {
            robot->ienc[RIGHT_ARM]->getEncoders(encoders_master);
            robot->ienc[LEFT_ARM] ->getEncoders(encoders_slave);
            for (int i=jjj; i<5; i++)
            {
                robot->idir[LEFT_ARM]->setPosition(i,encoders_master[i]);
            }

        }
    }

    void change_master()
    {
        left_arm_master=(!left_arm_master);
        if (left_arm_master)
        {
            for (int i=jjj; i<5; i++)
            {
                robot->icmd[LEFT_ARM]->setControlMode(i, VOCAB_CM_TORQUE);
                robot->icmd[RIGHT_ARM]->setControlMode(i, VOCAB_CM_POSITION_DIRECT);
                if (stiff==false) robot->iint[RIGHT_ARM]->setInteractionMode(i,VOCAB_IM_COMPLIANT);
                else              robot->iint[RIGHT_ARM]->setInteractionMode(i,VOCAB_IM_STIFF);
            }
        }
        else
        {
            for (int i=jjj; i<5; i++)
            {
                robot->icmd[LEFT_ARM]->setControlMode(i, VOCAB_CM_POSITION_DIRECT);
                if (stiff==false) robot->iint[LEFT_ARM]->setInteractionMode(i,VOCAB_IM_COMPLIANT);
                else              robot->iint[LEFT_ARM]->setInteractionMode(i,VOCAB_IM_STIFF);
                robot->icmd[RIGHT_ARM]->setControlMode(i, VOCAB_CM_TORQUE);
            }
        }
    }

    void closePort(Contactable *_port)
    {
        if (_port)
        {
            _port->interrupt();
            _port->close();

            delete _port;
            _port = 0;
        }
    }

    virtual void threadRelease()
    {  
        for (int i=0; i<5; i++)
        {
            robot->icmd[LEFT_ARM] ->setControlMode(i, VOCAB_CM_POSITION);
            robot->icmd[RIGHT_ARM]->setControlMode(i, VOCAB_CM_POSITION);
            robot->iint[LEFT_ARM] ->setInteractionMode(i,VOCAB_IM_STIFF);
            robot->iint[RIGHT_ARM]->setInteractionMode(i,VOCAB_IM_STIFF);
        }
        closePort(port_skin_contacts);
        closePort(port_left_arm);
        closePort(port_right_arm);
    }
};

    

class CtrlModule: public RFModule
{
    public:
    CtrlThread       *control_thr;
    CtrlModule();

    virtual bool configure(ResourceFinder &rf)
    {
        int rate = rf.check("period",Value(20)).asInt();
        control_thr=new CtrlThread(rate,rf);
        if (!control_thr->start())
        {
            delete control_thr;
            return false;
        }
        return true;
    }

    virtual double getPeriod()    { return 1.0;  }
    virtual bool   updateModule() { return true; }
    virtual bool   close()
    {
        if (control_thr)
        {
            control_thr->stop();
            delete control_thr;
        }
        return true;
    }
    bool respond(const Bottle& command, Bottle& reply) 
    {
        yInfo("rpc respond, still to be implemented\n");
        Bottle cmd;
        reply.clear(); 
        
        return true;
    }
};

CtrlModule::CtrlModule()
{

}

int main(int argc, char * argv[])
{
    ResourceFinder rf;
    rf.configure(argc,argv);
    //rf.setDefaultContext("empty");
    //rf.setDefaultConfigFile("empty");

    if (rf.check("help"))
    {
        yInfo("help not yet implemented\n");
    }

    //initialize yarp network
    Network yarp;

    if (!yarp.checkNetwork())
    {
        yError("Sorry YARP network does not seem to be available, is the yarp server available?\n");
        return 1;
    }

    CtrlModule mod;

    return mod.runModule(rf);
}


