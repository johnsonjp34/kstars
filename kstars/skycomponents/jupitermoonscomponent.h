/***************************************************************************
                          jupitermoonscomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/13/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef JUPITERMOONSCOMPONENT_H
#define JUPITERMOONSCOMPONENT_H

#include "skycomponent.h"

class SkyComposite;
class SolarSystemSingleComponent;
class KStarsData;
class SkyMap;
class KSNumbers;
class JupiterMoons;
class SkyLabeler;

/**
	*@class JupiterMoonsComponent
	*Represents the jupitermoons on the sky map.
	
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class JupiterMoonsComponent : public SkyComponent
{
public:

    /**
     *@short Constructor
     *@p parent pointer to the parent SkyComposite
     */
    JupiterMoonsComponent( SkyComponent *parent, SolarSystemSingleComponent *jup, bool (*visibleMethod)() );

    /**
     *@short Destructor
     */
    virtual ~JupiterMoonsComponent();

    /**
     *@short Draw the Jovian moons on the sky map
     *@p map Pointer to the SkyMap object
     *@p psky Reference to the QPainter on which to paint
     */
    virtual void draw( KStars *ks, QPainter& psky );

    /**
     *@short Initialize the Jovian moons
     *@p data Pointer to the KStarsData object
     */
    virtual void init(KStarsData *data);

    virtual void update( KStarsData *data, KSNumbers *num );
    virtual void updateMoons( KStarsData *data, KSNumbers *num );

private:

    JupiterMoons *jmoons;
    SolarSystemSingleComponent *m_Jupiter;
};

#endif
