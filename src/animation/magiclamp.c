/*
 * Animation plugin for compiz/beryl
 *
 * animation.c
 *
 * Copyright : (C) 2006 Erkin Bahceci
 * E-mail    : erkinbah@gmail.com
 *
 * Based on Wobbly and Minimize plugins by
 *           : David Reveman
 * E-mail    : davidr@novell.com>
 *
 * Particle system added by : (C) 2006 Dennis Kasprzyk
 * E-mail                   : onestone@beryl-project.org
 *
 * Beam-Up added by : Florencio Guimaraes
 * E-mail           : florencio@nexcorp.com.br
 *
 * Hexagon tessellator added by : Mike Slegeir
 * E-mail                       : mikeslegeir@mail.utexas.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * TODO:
 *
 * - Auto direction option: Close in opposite direction of opening
 * - Proper side surface normals for lighting
 * - decoration shadows
 *   - shadow quad generation
 *   - shadow texture coords (from clip tex. matrices)
 *   - draw shadows
 *   - fade in shadows
 *
 * - Voronoi tessellation
 * - Brick tessellation
 * - Triangle tessellation
 * - Hexagonal tessellation
 *
 * Effects:
 * - Circular action for tornado type fx
 * - Tornado 3D (especially for minimize)
 * - Helix 3D (hor. strips descend while they rotate and fade in)
 * - Glass breaking 3D
 *   - Gaussian distr. points (for gradually increasing polygon size
 *                           starting from center or near mouse pointer)
 *   - Drawing cracks
 *   - Gradual cracking
 *
 * - fix slowness during transparent cube with <100 opacity
 * - fix occasional wrong side color in some windows
 * - fix on top windows and panels
 *   (These two only matter for viewing during Rotate Cube.
 *    All windows should be painted with depth test on
 *    like 3d-plugin does)
 * - play better with rotate (fix cube face drawn on top of polygons
 *   after 45 deg. rotation)
 *
 */
#include "animation.h"


void
fxMagicLampInitGrid(AnimScreen * as,
					 WindowEvent forWindowEvent,
					 int *gridWidth, int *gridHeight)
{
	*gridWidth = 2;
	*gridHeight = as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_GRID_RES].value.i;
}
void
fxMagicLampVacuumInitGrid(AnimScreen * as, WindowEvent forWindowEvent,
					 int *gridWidth, int *gridHeight)
{
	*gridWidth = 2;
	*gridHeight = as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_VACUUM_GRID_RES].value.i;
}

void fxMagicLampInit(CompScreen * s, CompWindow * w)
{
	ANIM_SCREEN(s);
	ANIM_WINDOW(w);

	Model *model = aw->model;

	int screenHeight = s->height;
	Bool minimizeToTop = (WIN_Y(w) + WIN_H(w) / 2) >
			(aw->icon.y + aw->icon.height / 2);
	int maxWaves;
	float waveAmpMin, waveAmpMax;

	if (aw->curAnimEffect == AnimEffectMagicLamp)
	{
		maxWaves = as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_MAX_WAVES].value.i;
		waveAmpMin =
				as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_WAVE_AMP_MIN].value.f;
		waveAmpMax =
				as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_WAVE_AMP_MAX].value.f;
	}
	else
	{
		maxWaves = as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_VACUUM_MAX_WAVES].value.i;
		waveAmpMin =
				as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_VACUUM_WAVE_AMP_MIN].value.f;
		waveAmpMax =
				as->opt[ANIM_SCREEN_OPTION_MAGIC_LAMP_VACUUM_WAVE_AMP_MAX].value.f;
	}
	if (waveAmpMax < waveAmpMin)
		waveAmpMax = waveAmpMin;

	if (maxWaves > 0)
	{
		float distance;

		if (minimizeToTop)
			distance = WIN_Y(w) + WIN_H(w) - aw->icon.y;
		else
			distance = aw->icon.y - WIN_Y(w);

		// Initialize waves
		model->magicLampWaveCount =
				1 + (float)maxWaves *distance / screenHeight;

		if (!(model->magicLampWaves))
			model->magicLampWaves =
					calloc(1, model->magicLampWaveCount * sizeof(WaveParam));

		int ampDirection = (RAND_FLOAT() < 0.5 ? 1 : -1);
		int i;
		float minHalfWidth = 0.22f;
		float maxHalfWidth = 0.38f;

		for (i = 0; i < model->magicLampWaveCount; i++)
		{
			model->magicLampWaves[i].amp =
					ampDirection * (waveAmpMax - waveAmpMin) *
					rand() / RAND_MAX + ampDirection * waveAmpMin;
			model->magicLampWaves[i].halfWidth =
					RAND_FLOAT() * (maxHalfWidth -
									minHalfWidth) + minHalfWidth;

			// avoid offset at top and bottom part by added waves
			float availPos = 1 - 2 * model->magicLampWaves[i].halfWidth;
			float posInAvailSegment = 0;

			if (i > 0)
				posInAvailSegment =
						(availPos /
						 model->magicLampWaveCount) * rand() / RAND_MAX;

			model->magicLampWaves[i].pos =
					(posInAvailSegment +
					 i * availPos / model->magicLampWaveCount +
					 model->magicLampWaves[i].halfWidth);

			// switch wave direction
			ampDirection *= -1;
		}
	}
	else
		model->magicLampWaveCount = 0;
}

void
fxMagicLampModelStepObject(CompWindow * w,
						   Model * model,
						   Object * object,
						   float forwardProgress, Bool minimizeToTop)
{
	ANIM_WINDOW(w);

	float iconCloseEndY;
	float iconFarEndY;
	float winFarEndY;
	float winVisibleCloseEndY;

	if (minimizeToTop)
	{
		iconFarEndY = aw->icon.y;
		iconCloseEndY = aw->icon.y + aw->icon.height;
		winFarEndY = WIN_Y(w) + WIN_H(w);
		winVisibleCloseEndY = WIN_Y(w);
		if (winVisibleCloseEndY < iconCloseEndY)
			winVisibleCloseEndY = iconCloseEndY;
	}
	else
	{
		iconFarEndY = aw->icon.y + aw->icon.height;
		iconCloseEndY = aw->icon.y;
		winFarEndY = WIN_Y(w);
		winVisibleCloseEndY = WIN_Y(w) + WIN_H(w);
		if (winVisibleCloseEndY > iconCloseEndY)
			winVisibleCloseEndY = iconCloseEndY;
	}

	float preShapePhaseEnd = 0.17f;
	float stretchPhaseEnd =
			preShapePhaseEnd + (1 - preShapePhaseEnd) *
			(iconCloseEndY -
			 winVisibleCloseEndY) / ((iconCloseEndY - winFarEndY) +
									 (iconCloseEndY - winVisibleCloseEndY));
	if (stretchPhaseEnd < preShapePhaseEnd + 0.1)
		stretchPhaseEnd = preShapePhaseEnd + 0.1;

	float origx = w->attrib.x + (WIN_W(w) * object->gridPosition.x -
								 w->output.left) * model->scale.x;
	float origy = w->attrib.y + (WIN_H(w) * object->gridPosition.y -
								 w->output.top) * model->scale.y;

	float iconShadowLeft =
			((float)(w->output.left - w->input.left)) * 
			aw->icon.width / w->width;
	float iconShadowRight =
			((float)(w->output.right - w->input.right)) * 
			aw->icon.width / w->width;
	float iconx =
			(aw->icon.x - iconShadowLeft) + 
			(aw->icon.width + iconShadowLeft + iconShadowRight) *
			object->gridPosition.x;
	float icony = aw->icon.y + aw->icon.height * object->gridPosition.y;

	if (forwardProgress < preShapePhaseEnd)
	{
		float preShapeProgress = forwardProgress / preShapePhaseEnd;
		float fx = (iconCloseEndY - object->position.y) / 
			       (iconCloseEndY - winFarEndY);
		float fy = (sigmoid(fx) - sigmoid(0)) / (sigmoid(1) - sigmoid(0));
		int i;
		float targetx = fy * (origx - iconx) + iconx;

		for (i = 0; i < model->magicLampWaveCount; i++)
		{
			float cosfx =
					(fx -
					 model->magicLampWaves[i].pos) /
					model->magicLampWaves[i].halfWidth;
			if (cosfx < -1 || cosfx > 1)
				continue;
			targetx +=
					model->magicLampWaves[i].amp * model->scale.x *
					(cos(cosfx * M_PI) + 1) / 2;
		}
		object->position.x =
				(1 - preShapeProgress) * origx + preShapeProgress * targetx;
		object->position.y = origy;
	}
	else
	{
		float stretchedPos;

		if (minimizeToTop)
			stretchedPos =
					object->gridPosition.y * origy +
					(1 - object->gridPosition.y) * icony;
		else
			stretchedPos =
					(1 - object->gridPosition.y) * origy +
					object->gridPosition.y * icony;

		if (forwardProgress < stretchPhaseEnd)
		{
			float stretchProgress =
					(forwardProgress - preShapePhaseEnd) /
					(stretchPhaseEnd - preShapePhaseEnd);

			object->position.y =
					(1 - stretchProgress) * origy +
					stretchProgress * stretchedPos;
		}
		else
		{
			float postStretchProgress =
					(forwardProgress - stretchPhaseEnd) / (1 -
														   stretchPhaseEnd);

			object->position.y =
					(1 - postStretchProgress) *
					stretchedPos +
					postStretchProgress *
					(stretchedPos + (iconCloseEndY - winFarEndY));
		}
		float fx =
				(iconCloseEndY - object->position.y) / (iconCloseEndY -
														winFarEndY);
		float fy = (sigmoid(fx) - sigmoid(0)) / (sigmoid(1) - sigmoid(0));

		int i;

		object->position.x = fy * (origx - iconx) + iconx;
		for (i = 0; i < model->magicLampWaveCount; i++)
		{
			float cosfx =
					(fx -
					 model->magicLampWaves[i].pos) /
					model->magicLampWaves[i].halfWidth;
			if (cosfx < -1 || cosfx > 1)
				continue;
			object->position.x +=
					model->magicLampWaves[i].amp * model->scale.x *
					(cos(cosfx * M_PI) + 1) / 2;
		}
	}
	if (minimizeToTop)
	{
		if (object->position.y < iconFarEndY)
			object->position.y = iconFarEndY;
	}
	else
	{
		if (object->position.y > iconFarEndY)
			object->position.y = iconFarEndY;
	}
}

void fxMagicLampModelStep(CompScreen * s, CompWindow * w, float time)
{
	int i, j, steps;

	ANIM_SCREEN(s);
	ANIM_WINDOW(w);

	Model *model = aw->model;

	float timestep = (s->slowAnimations ? 2 :	// For smooth slow-mo (refer to display.c)
					  as->opt[ANIM_SCREEN_OPTION_TIME_STEP].value.i);

	aw->remainderSteps += time / timestep;
	steps = floor(aw->remainderSteps);
	aw->remainderSteps -= steps;

	if (!steps && aw->animRemainingTime < aw->animTotalTime)
		return;
	steps = MAX(1, steps);

	Bool minimizeToTop = (WIN_Y(w) + WIN_H(w) / 2) >
			(aw->icon.y + aw->icon.height / 2);

	for (j = 0; j < steps; j++)
	{
		float forwardProgress =
				1 - (aw->animRemainingTime - timestep) /
				(aw->animTotalTime - timestep);
		if (aw->curWindowEvent == WindowEventUnminimize ||
			aw->curWindowEvent == WindowEventCreate)
			forwardProgress = 1 - forwardProgress;

		for (i = 0; i < model->numObjects; i++)
		{
			fxMagicLampModelStepObject(w, model,
									   &model->objects[i],
									   forwardProgress, minimizeToTop);
		}
		aw->animRemainingTime -= timestep;
		if (aw->animRemainingTime <= 0)
		{
			aw->animRemainingTime = 0;	// avoid sub-zero values
			break;
		}
	}
	modelCalcBounds(model);
}