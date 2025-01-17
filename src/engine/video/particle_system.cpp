///////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2011 by The Allacrost Project
//            Copyright (C) 2012-2016 by Bertram (Valyria Tear)
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software
// and you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
///////////////////////////////////////////////////////////////////////////////

/** ***************************************************************************
*** \file    particle_system.cpp
*** \author  Raj Sharma, roos@allacrost.org
*** \author  Yohann Ferreira, yohann ferreira orange fr
*** \brief   Source file for particle system
*** **************************************************************************/

#include "particle_system.h"

#include "particle_keyframe.h"
#include "engine/video/video.h"

#include "utils/utils_random.h"

#include <cassert>

using namespace vt_utils;
using namespace vt_video;
using namespace vt_common;

namespace vt_mode_manager
{

bool ParticleSystem::_Create(ParticleSystemDef *sys_def)
{
    // Make sure the system def is valid before initializing.
    if(!sys_def) {
        _Destroy();
        return false;
    }

    // The pointer is set, yet it should never be deleted here.
    _system_def = sys_def;
    _num_particles = 0;

    _particles.resize(_system_def->max_particles);
    _particle_vertices.resize(_system_def->max_particles * 4);
    _particle_texcoords.resize(_system_def->max_particles * 4);
    _particle_colors.resize(_system_def->max_particles * 4);

    _alive = true;
    _stopped = false;
    _age = 0.0f;

    size_t num_frames = _system_def->animation_frame_filenames.size();

    for(size_t j = 0; j < num_frames; ++j) {
        int32_t frame_time;
        if(j < _system_def->animation_frame_times.size())
            frame_time = _system_def->animation_frame_times[j];
        else if(_system_def->animation_frame_times.empty())
            frame_time = 0;
        else
            frame_time = _system_def->animation_frame_times.back();

        _animation.AddFrame(_system_def->animation_frame_filenames[j], frame_time);
    }

    return true;
}

void ParticleSystem::Draw()
{
    if (!_alive || !_system_def->enabled || _age < _system_def->emitter._start_time || _num_particles <= 0)
        return;

    // Set the blending parameters.
    if (_system_def->blend_mode == VIDEO_NO_BLEND) {
        VideoManager->DisableBlending();
    } else {
        VideoManager->EnableBlending();

        if (_system_def->blend_mode == VIDEO_BLEND)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive.
    }

    if (_system_def->use_stencil) {
        VideoManager->EnableStencilTest();
        glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    } else if (_system_def->modify_stencil) {
        VideoManager->EnableStencilTest();

        if (_system_def->stencil_op == VIDEO_STENCIL_OP_INCREASE)
            glStencilOp(GL_INCR, GL_KEEP, GL_KEEP);
        else if (_system_def->stencil_op == VIDEO_STENCIL_OP_DECREASE)
            glStencilOp(GL_DECR, GL_KEEP, GL_KEEP);
        else if (_system_def->stencil_op == VIDEO_STENCIL_OP_ZERO)
            glStencilOp(GL_ZERO, GL_KEEP, GL_KEEP);
        else
            glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);

        glStencilFunc(GL_NEVER, 1, 0xFFFFFFFF);
    } else {
        VideoManager->DisableStencilTest();
    }

    VideoManager->EnableTexture2D();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    StillImage* id = _animation.GetFrame(_animation.GetCurrentFrameIndex());
    private_video::ImageTexture* img = id->_image_texture;
    TextureManager->_BindTexture(img->texture_sheet->tex_id);

    float frame_progress = _animation.GetPercentProgress();

    float u1 = img->u1;
    float u2 = img->u2;
    float v1 = img->v1;
    float v2 = img->v2;

    float img_width  = static_cast<float>(img->width);
    float img_height = static_cast<float>(img->height);

    float img_width_half = img_width * 0.5f;
    float img_height_half = img_height * 0.5f;

    // Fill the vertex array.
    if (_system_def->rotation_used) {
        int32_t v = 0;

        for (int32_t j = 0; j < _num_particles; ++j) {
            float scaled_width_half  = img_width_half * _particles[j].size.x;
            float scaled_height_half = img_height_half * _particles[j].size.y;

            float rotation_angle = _particles[j].rotation_angle;

            if(_system_def->rotate_to_velocity) {
                // Calculate the angle based on the velocity.
                rotation_angle += UTILS_HALF_PI + atan2f(_particles[j].combined_velocity.y,
                                                         _particles[j].combined_velocity.x);

                // Calculate the scaling due to speed.
                if(_system_def->speed_scale_used) {
                    // Speed is the magnitude of velocity.
                    float speed = sqrtf(_particles[j].combined_velocity.x * _particles[j].combined_velocity.x
                                        + _particles[j].combined_velocity.y * _particles[j].combined_velocity.y);
                    float scale_factor = _system_def->speed_scale * speed;

                    if (scale_factor < _system_def->min_speed_scale)
                        scale_factor = _system_def->min_speed_scale;
                    if (scale_factor > _system_def->max_speed_scale)
                        scale_factor = _system_def->max_speed_scale;

                    scaled_height_half *= scale_factor;
                }
            }

            // The upper-left vertex.
            _particle_vertices[v]._x = -scaled_width_half;
            _particle_vertices[v]._y = -scaled_height_half;
            RotatePoint(_particle_vertices[v]._x, _particle_vertices[v]._y, rotation_angle);
            _particle_vertices[v]._x += _particles[j].pos.x;
            _particle_vertices[v]._y += _particles[j].pos.y;
            ++v;

            // The upper-right vertex.
            _particle_vertices[v]._x = scaled_width_half;
            _particle_vertices[v]._y = -scaled_height_half;
            RotatePoint(_particle_vertices[v]._x, _particle_vertices[v]._y, rotation_angle);
            _particle_vertices[v]._x += _particles[j].pos.x;
            _particle_vertices[v]._y += _particles[j].pos.y;
            ++v;

            // The lower-right vertex.
            _particle_vertices[v]._x = scaled_width_half;
            _particle_vertices[v]._y = scaled_height_half;
            RotatePoint(_particle_vertices[v]._x, _particle_vertices[v]._y, rotation_angle);
            _particle_vertices[v]._x += _particles[j].pos.x;
            _particle_vertices[v]._y += _particles[j].pos.y;
            ++v;

            // The lower-left vertex.
            _particle_vertices[v]._x = -scaled_width_half;
            _particle_vertices[v]._y = scaled_height_half;
            RotatePoint(_particle_vertices[v]._x, _particle_vertices[v]._y, rotation_angle);
            _particle_vertices[v]._x += _particles[j].pos.x;
            _particle_vertices[v]._y += _particles[j].pos.y;
            ++v;
        }
    } else {
        int32_t v = 0;

        for (int32_t j = 0; j < _num_particles; ++j) {
            float scaled_width_half  = img_width_half * _particles[j].size.x;
            float scaled_height_half = img_height_half * _particles[j].size.y;

            // The upper-left vertex.
            _particle_vertices[v]._x = _particles[j].pos.x - scaled_width_half;
            _particle_vertices[v]._y = _particles[j].pos.y - scaled_height_half;
            ++v;

            // The upper-right vertex.
            _particle_vertices[v]._x = _particles[j].pos.x + scaled_width_half;
            _particle_vertices[v]._y = _particles[j].pos.y - scaled_height_half;
            ++v;

            // The lower-right vertex.
            _particle_vertices[v]._x = _particles[j].pos.x + scaled_width_half;
            _particle_vertices[v]._y = _particles[j].pos.y + scaled_height_half;
            ++v;

            // lower-left vertex
            _particle_vertices[v]._x = _particles[j].pos.x - scaled_width_half;
            _particle_vertices[v]._y = _particles[j].pos.y + scaled_height_half;
            ++v;
        }
    }

    // Fill the color array.

    int32_t c = 0;
    for (int32_t j = 0; j < _num_particles; ++j) {
        Color color = _particles[j].color;

        if (_system_def->smooth_animation)
            color = color * (1.0f - frame_progress);

        _particle_colors[c] = color;
        ++c;
        _particle_colors[c] = color;
        ++c;
        _particle_colors[c] = color;
        ++c;
        _particle_colors[c] = color;
        ++c;
    }

    // Fill the texture coordinate array.

    int32_t t = 0;
    for (int32_t j = 0; j < _num_particles; ++j) {
        // The upper-left vertex.
        _particle_texcoords[t]._t0 = u1;
        _particle_texcoords[t]._t1 = v1;
        ++t;

        // The upper-right vertex.
        _particle_texcoords[t]._t0 = u2;
        _particle_texcoords[t]._t1 = v1;
        ++t;

        // The lower-right vertex.
        _particle_texcoords[t]._t0 = u2;
        _particle_texcoords[t]._t1 = v2;
        ++t;

        // The lower-left vertex.
        _particle_texcoords[t]._t0 = u1;
        _particle_texcoords[t]._t1 = v2;
        ++t;
    }

    // Load the sprite shader program.
    gl::ShaderProgram* shader_program = VideoManager->LoadShaderProgram(gl::shader_programs::Sprite);
    assert(shader_program != nullptr);

    // Draw the particle system.
    VideoManager->DrawParticleSystem(shader_program,
                                     reinterpret_cast<float*>(&_particle_vertices[0]),
                                     reinterpret_cast<float*>(&_particle_texcoords[0]),
                                     reinterpret_cast<float*>(&_particle_colors[0]),
                                     _num_particles * 4);

    if (_system_def->smooth_animation) {
        uint32_t findex = _animation.GetCurrentFrameIndex();
        findex = (findex + 1) % _animation.GetNumFrames();

        StillImage *id2 = _animation.GetFrame(findex);
        private_video::ImageTexture *img2 = id2->_image_texture;
        TextureManager->_BindTexture(img2->texture_sheet->tex_id);

        u1 = img2->u1;
        u2 = img2->u2;
        v1 = img2->v1;
        v2 = img2->v2;

        t = 0;
        for (int32_t j = 0; j < _num_particles; ++j) {
            // The upper-left vertex.
            _particle_texcoords[t]._t0 = u1;
            _particle_texcoords[t]._t1 = v1;
            ++t;

            // The upper-right vertex.
            _particle_texcoords[t]._t0 = u2;
            _particle_texcoords[t]._t1 = v1;
            ++t;

            // The lower-right vertex.
            _particle_texcoords[t]._t0 = u2;
            _particle_texcoords[t]._t1 = v2;
            ++t;

            // The lower-left vertex.
            _particle_texcoords[t]._t0 = u1;
            _particle_texcoords[t]._t1 = v2;
            ++t;
        }

        c = 0;
        for (int32_t j = 0; j < _num_particles; ++j) {
            Color color = _particles[j].color;
            color = color * frame_progress;

            _particle_colors[c] = color;
            ++c;
            _particle_colors[c] = color;
            ++c;
            _particle_colors[c] = color;
            ++c;
            _particle_colors[c] = color;
            ++c;
        }

        // Draw the particle system.
        VideoManager->DrawParticleSystem(shader_program,
                                         reinterpret_cast<float*>(&_particle_vertices[0]),
                                         reinterpret_cast<float*>(&_particle_texcoords[0]),
                                         reinterpret_cast<float*>(&_particle_colors[0]),
                                         _num_particles * 4);
    }

    // Unload the shader program.
    VideoManager->UnloadShaderProgram();
}

//-----------------------------------------------------------------------------
// Update: updates particle positions and properties, and emits/kills particles
//-----------------------------------------------------------------------------

void ParticleSystem::Update(float frame_time, const EffectParameters &params)
{
    if(!_alive || !_system_def->enabled)
        return;

    _age += frame_time;

    if(_age < _system_def->emitter._start_time) {
        _last_update_time = _age;
        return;
    }

    _animation.Update();

    // update properties of existing particles
    _UpdateParticles(frame_time, params);

    // figure out how many particles need to be emitted this frame
    int32_t num_particles_to_emit = 0;
    if(!_stopped) {
        if(_system_def->emitter._emitter_mode == EMITTER_MODE_ALWAYS) {
            num_particles_to_emit = _system_def->max_particles - _num_particles;
        } else if(_system_def->emitter._emitter_mode != EMITTER_MODE_BURST) {
            float time_low  = _last_update_time * _system_def->emitter._emission_rate;
            float time_high = _age * _system_def->emitter._emission_rate;

            time_low  = floorf(time_low);
            time_high = ceilf(time_high);

            num_particles_to_emit = static_cast<int32_t>(time_high - time_low) - 1;

            if(num_particles_to_emit + _num_particles > _system_def->max_particles)
                num_particles_to_emit = _system_def->max_particles - _num_particles;
        } else {
            num_particles_to_emit = _system_def->max_particles;
        }
    }

    // kill expired particles. If there are particles waiting to be emitted, then instead of
    // killing, just respawn the expired particle since this is much more efficient
    _KillParticles(num_particles_to_emit, params);

    // if there are still any particles waiting to be emitted, emit them
    _EmitParticles(num_particles_to_emit, params);

    // stop the particle system immediately if burst is used
    if(_system_def->emitter._emitter_mode == EMITTER_MODE_BURST)
        Stop();

    // stop the system if it's past its lifetime. Note that the only mode in which
    // the system lifetime is applicable is ONE_SHOT mode
    if(_system_def->emitter._emitter_mode == EMITTER_MODE_ONE_SHOT) {
        if(_age > _system_def->system_lifetime)
            _stopped = true;
    }

    // check if the system is dead
    if(_num_particles == 0 && _stopped)
        _alive = false;

    _last_update_time = _age;
}

void ParticleSystem::_Destroy()
{
    _num_particles = 0;
    _age = 0.0f;
    _last_update_time = 0.0f;

    _alive = false;
    _stopped = false;

    _particles.clear();
    _particle_vertices.clear();
    // Don't delete it, since it's handled by the ParticleEffectDef
    _system_def = 0;
}

void ParticleSystem::_UpdateParticles(float t, const EffectParameters &params)
{
    for(int32_t j = 0; j < _num_particles; ++j) {
        // calculate a time for the particle from 0 to 1 since this is what
        // the keyframes are based on
        float scaled_time = _particles[j].time / _particles[j].lifetime;

        // figure out which keyframe we're on
        if(_particles[j].next_keyframe) {
            ParticleKeyframe *old_next = _particles[j].next_keyframe;

            // check if we need to advance the keyframe
            if(scaled_time >= _particles[j].next_keyframe->time) {
                // figure out what keyframe we're on
                size_t num_keyframes = _system_def->keyframes.size();

                size_t k;
                for(k = 0; k < num_keyframes; ++k) {
                    if(_system_def->keyframes[k].time > scaled_time) {
                        _particles[j].current_keyframe = &_system_def->keyframes[k - 1];
                        _particles[j].next_keyframe    = &_system_def->keyframes[k];
                        break;
                    }
                }

                // if we didn't find any keyframe whose time is larger than this
                // particle's time, then we are on the last one
                if(k == num_keyframes) {
                    _particles[j].current_keyframe = &_system_def->keyframes[k - 1];
                    _particles[j].next_keyframe = nullptr;

                    // set all of the keyframed properties to the value stored in the last
                    // keyframe
                    _particles[j].color          = _particles[j].current_keyframe->color;
                    _particles[j].rotation_speed = _particles[j].current_keyframe->rotation_speed;
                    _particles[j].size         = _particles[j].current_keyframe->size;
                }

                // if we skipped ahead only 1 keyframe, then inherit the current variations
                // from the next ones
                if(_particles[j].current_keyframe == old_next) {
                    _particles[j].current_color_variation = _particles[j].next_color_variation;
                    _particles[j].current_rotation_speed_variation = _particles[j].next_rotation_speed_variation;
                    _particles[j].current_size_variation = _particles[j].next_size_variation;
                } else {
                    _particles[j].current_rotation_speed_variation = RandomFloat(-_particles[j].current_keyframe->rotation_speed_variation, _particles[j].current_keyframe->rotation_speed_variation);
                    for(int32_t c = 0; c < 4; ++c)
                        _particles[j].current_color_variation[c] = RandomFloat(-_particles[j].current_keyframe->color_variation[c], _particles[j].current_keyframe->color_variation[c]);
                    _particles[j].current_size_variation.x = RandomFloat(-_particles[j].current_keyframe->size_variation.x,
                                                                         _particles[j].current_keyframe->size_variation.x);
                    _particles[j].current_size_variation.y = RandomFloat(-_particles[j].current_keyframe->size_variation.y,
                                                                         _particles[j].current_keyframe->size_variation.y);
                }

                // if there is a next keyframe, generate variations for it
                if(_particles[j].next_keyframe) {
                    _particles[j].next_rotation_speed_variation = RandomFloat(-_particles[j].next_keyframe->rotation_speed_variation, _particles[j].next_keyframe->rotation_speed_variation);
                    for(int32_t c = 0; c < 4; ++c)
                        _particles[j].next_color_variation[c] = RandomFloat(-_particles[j].next_keyframe->color_variation[c], _particles[j].next_keyframe->color_variation[c]);
                    _particles[j].next_size_variation.x = RandomFloat(-_particles[j].next_keyframe->size_variation.x,
                                                                      _particles[j].next_keyframe->size_variation.x);
                    _particles[j].next_size_variation.y = RandomFloat(-_particles[j].next_keyframe->size_variation.y,
                                                                      _particles[j].next_keyframe->size_variation.y);
                }
            }
        }

        // if we aren't already at the last keyframe, interpolate to figure out the
        // current keyframed properties
        if(_particles[j].next_keyframe) {
            // figure out how far we are from the current to the next (0.0 to 1.0)
            float cur_a = (scaled_time - _particles[j].current_keyframe->time) /
                          (_particles[j].next_keyframe->time - _particles[j].current_keyframe->time);

            _particles[j].rotation_speed = Lerp(_particles[j].current_keyframe->rotation_speed
                                                + _particles[j].current_rotation_speed_variation,
                                                _particles[j].next_keyframe->rotation_speed
                                                + _particles[j].next_rotation_speed_variation, cur_a);
            _particles[j].size.x         = Lerp(_particles[j].current_keyframe->size.x
                                                + _particles[j].current_size_variation.x,
                                                _particles[j].next_keyframe->size.x
                                                + _particles[j].next_size_variation.x, cur_a);
            _particles[j].size.y         = Lerp(_particles[j].current_keyframe->size.y
                                                + _particles[j].current_size_variation.y,
                                                _particles[j].next_keyframe->size.y
                                                + _particles[j].next_size_variation.y, cur_a);
            _particles[j].color[0]       = Lerp(_particles[j].current_keyframe->color[0]
                                                + _particles[j].current_color_variation[0],
                                                _particles[j].next_keyframe->color[0]
                                                + _particles[j].next_color_variation[0], cur_a);
            _particles[j].color[1]       = Lerp(_particles[j].current_keyframe->color[1]
                                                + _particles[j].current_color_variation[1],
                                                _particles[j].next_keyframe->color[1]
                                                + _particles[j].next_color_variation[1], cur_a);
            _particles[j].color[2]       = Lerp(_particles[j].current_keyframe->color[2]
                                                + _particles[j].current_color_variation[2],
                                                _particles[j].next_keyframe->color[2]
                                                + _particles[j].next_color_variation[2], cur_a);
            _particles[j].color[3]       = Lerp(_particles[j].current_keyframe->color[3]
                                                + _particles[j].current_color_variation[3],
                                                _particles[j].next_keyframe->color[3]
                                                + _particles[j].next_color_variation[3], cur_a);
        }

        _particles[j].rotation_angle += _particles[j].rotation_speed * _particles[j].rotation_direction * t;

        Position2D wind_velocity = _particles[j].wind_velocity;

        _particles[j].combined_velocity.x = _particles[j].velocity.x + wind_velocity.x;
        _particles[j].combined_velocity.y = _particles[j].velocity.y + wind_velocity.y;

        if(_system_def->wave_motion_used && _particles[j].wave_half_amplitude > 0.0f) {
            // find the magnitude of the wave velocity
            float wave_speed = _particles[j].wave_half_amplitude
                               * sinf(_particles[j].wave_length_coefficient * _particles[j].time);

            // now the wave velocity is just that wave speed times the particle's tangential vector
            // Note the inverted x and y assignments
            Position2D tangent(-_particles[j].combined_velocity.y,
                               _particles[j].combined_velocity.x);
            float speed = sqrtf(tangent.GetLength2());
            tangent.x /= speed;
            tangent.y /= speed;

            Position2D wave_velocity(tangent.x * wave_speed,
                                     tangent.y * wave_speed);

            _particles[j].combined_velocity.x += wave_velocity.x;
            _particles[j].combined_velocity.y += wave_velocity.y;
        }

        _particles[j].pos.x += (_particles[j].combined_velocity.x) * t;
        _particles[j].pos.y += (_particles[j].combined_velocity.y) * t;


        // client-specified acceleration (dv = a * t)
        _particles[j].velocity.x += _particles[j].acceleration.x * t;
        _particles[j].velocity.y += _particles[j].acceleration.y * t;

        // radial acceleration: calculate unit vector from emitter center to this particle,
        // and scale by the radial acceleration, if there is any

        bool use_radial     = (_particles[j].radial_acceleration != 0.0f);
        bool use_tangential = (_particles[j].tangential_acceleration != 0.0f);

        if(use_radial || use_tangential) {
            // unit vector from attractor to particle
            Position2D attractor_to_particle;

            if(_system_def->user_defined_attractor) {
                attractor_to_particle.x = _particles[j].pos.x - params.attractor.x;
                attractor_to_particle.y = _particles[j].pos.y - params.attractor.y;
            } else {
                attractor_to_particle.x = _particles[j].pos.x - _system_def->emitter._center.x;
                attractor_to_particle.y = _particles[j].pos.y - _system_def->emitter._center.y;
            }

            float distance = sqrtf(attractor_to_particle.GetLength2());

            if(distance != 0.0f) {
                attractor_to_particle.x /= distance;
                attractor_to_particle.y /= distance;
            }

            // radial acceleration
            if(use_radial) {
                if(_system_def->attractor_falloff != 0.0f) {
                    float attraction = 1.0f - _system_def->attractor_falloff * distance;
                    if(attraction > 0.0f) {
                        _particles[j].velocity.x += attractor_to_particle.x
                                                    * _particles[j].radial_acceleration
                                                    * t * attraction;
                        _particles[j].velocity.y += attractor_to_particle.y
                                                    * _particles[j].radial_acceleration
                                                    * t * attraction;
                    }
                } else {
                    _particles[j].velocity.x += attractor_to_particle.x
                                                * _particles[j].radial_acceleration * t;
                    _particles[j].velocity.y += attractor_to_particle.y
                                                * _particles[j].radial_acceleration * t;
                }
            }

            // tangential acceleration
            if(use_tangential) {
                // tangent vector is simply perpendicular vector
                // Note the inversion of x and y
                Position2D tangent(-attractor_to_particle.y,
                                   attractor_to_particle.x);

                _particles[j].velocity.x += tangent.x * _particles[j].tangential_acceleration * t;
                _particles[j].velocity.y += tangent.y * _particles[j].tangential_acceleration * t;
            }
        }

        // damp the velocity

        if(_particles[j].damping != 1.0f) {
            _particles[j].velocity.x *= pow(_particles[j].damping, t);
            _particles[j].velocity.y *= pow(_particles[j].damping, t);
        }

        _particles[j].time += t;
    }
}


//-----------------------------------------------------------------------------
// _KillParticles: helper function to kill expired particles. The num parameter
//                 tells how many particles need to be emitted this frame.
//                 If possible, we try to respawn particles instead of killing
//                 and then emitting, because it is much more efficient.
//-----------------------------------------------------------------------------

void ParticleSystem::_KillParticles(int32_t &num, const EffectParameters &params)
{
    // check each active particle to see if it is expired
    for(int32_t j = 0; j < _num_particles; ++j) {
        if(_particles[j].time > _particles[j].lifetime) {
            if(num > 0) {
                // if we still have particles to emit, then instead of killing the particle,
                // respawn it as a new one
                _RespawnParticle(j, params);
                --num;
            } else {
                // kill the particle, i.e. move the particle at the end of the array to this
                // particle's spot, and decrement _num_particles
                if(j != _num_particles - 1)
                    _MoveParticle(_num_particles - 1, j);
                --_num_particles;
            }
        }
    }
}


//-----------------------------------------------------------------------------
// _EmitParticles: helper function, emits new particles
//-----------------------------------------------------------------------------

void ParticleSystem::_EmitParticles(int32_t num, const EffectParameters &params)
{
    // respawn 'num' new particles at the end of the array
    for(int32_t j = 0; j < num; ++j) {
        _RespawnParticle(_num_particles, params);
        ++_num_particles;
    }
}


//-----------------------------------------------------------------------------
// _MoveParticle: helper function, moves the data for a particle from src
//                to dest index in the array
//-----------------------------------------------------------------------------

void ParticleSystem::_MoveParticle(int32_t src, int32_t dest)
{
    _particles[dest] = _particles[src];
}


//-----------------------------------------------------------------------------
// _RespawnParticle: helper function to Update(), does the work of setting up
//                   the properties for a newly spawned particle
//-----------------------------------------------------------------------------

void ParticleSystem::_RespawnParticle(int32_t i, const EffectParameters &params)
{
    const ParticleEmitter &emitter = _system_def->emitter;

    switch(emitter._shape) {
    case EMITTER_SHAPE_POINT: {
        _particles[i].pos.x = emitter._pos.x;
        _particles[i].pos.y = emitter._pos.y;
        break;
    }
    case EMITTER_SHAPE_LINE: {
        _particles[i].pos.x = RandomFloat(emitter._pos.x, emitter._pos2.x);
        _particles[i].pos.y = RandomFloat(emitter._pos.y, emitter._pos2.y);
        break;
    }
    case EMITTER_SHAPE_CIRCLE: {
        float angle = RandomFloat(0.0f, UTILS_2PI);
        _particles[i].pos.x = emitter._radius * cosf(angle);
        _particles[i].pos.y = emitter._radius * sinf(angle);
        // Apply offset
        _particles[i].pos.x += emitter._pos.x;
        _particles[i].pos.y += emitter._pos.y;
        break;
    }
    case EMITTER_SHAPE_ELLIPSE: {
        float angle = RandomFloat(0.0f, UTILS_2PI);
        _particles[i].pos.x = emitter._pos.x * cosf(angle);
        _particles[i].pos.y = emitter._pos.y * sinf(angle);
        // Apply offset
        _particles[i].pos.x += emitter._pos2.x;
        _particles[i].pos.y += emitter._pos2.y;
        break;
    }
    case EMITTER_SHAPE_FILLED_CIRCLE: {
        float radius_squared = emitter._radius;
        radius_squared *= radius_squared;

        // use rejection sampling to choose a point within the circle
        // this may need to be replaced by a speedier algorithm later on
        do {
            float half_radius = emitter._radius * 0.5f;
            _particles[i].pos.x = RandomFloat(-half_radius, half_radius);
            _particles[i].pos.y = RandomFloat(-half_radius, half_radius);
        } while(_particles[i].pos.x * _particles[i].pos.x +
                _particles[i].pos.y * _particles[i].pos.y > radius_squared);
        // Apply offset
        _particles[i].pos.x += emitter._pos.x;
        _particles[i].pos.y += emitter._pos.y;
        break;
    }
    case EMITTER_SHAPE_FILLED_RECTANGLE: {
        _particles[i].pos.x = RandomFloat(emitter._pos.x, emitter._pos2.x);
        _particles[i].pos.y = RandomFloat(emitter._pos.y, emitter._pos2.y);
        break;
    }
    default:
        break;
    };


    _particles[i].pos.x += RandomFloat(-emitter._variation.x, emitter._variation.x);
    _particles[i].pos.y += RandomFloat(-emitter._variation.y, emitter._variation.y);

    if(params.orientation != 0.0f)
        RotatePoint(_particles[i].pos.x, _particles[i].pos.y, params.orientation);

    _particles[i].color = _system_def->keyframes[0].color;

    _particles[i].rotation_speed  = _system_def->keyframes[0].rotation_speed;
    _particles[i].time            = 0.0f;
    _particles[i].size            = _system_def->keyframes[0].size;

    if(_system_def->random_initial_angle)
        _particles[i].rotation_angle = RandomFloat(0.0f, UTILS_2PI);
    else
        _particles[i].rotation_angle = 0.0f;

    _particles[i].current_keyframe = &_system_def->keyframes[0];

    if(_system_def->keyframes.size() > 1)
        _particles[i].next_keyframe = &_system_def->keyframes[1];
    else
        _particles[i].next_keyframe = nullptr;

    float speed = _system_def->emitter._initial_speed;
    speed += RandomFloat(-emitter._initial_speed_variation, emitter._initial_speed_variation);

    if(_system_def->emitter._spin == EMITTER_SPIN_CLOCKWISE) {
        _particles[i].rotation_direction = 1.0f;
    } else if(_system_def->emitter._spin == EMITTER_SPIN_COUNTERCLOCKWISE) {
        _particles[i].rotation_direction = -1.0f;
    } else {
        _particles[i].rotation_direction = static_cast<float>(2 * (rand() % 2)) - 1.0f;
    }

    // figure out the orientation
    float angle = 0.0f;

    if(emitter._omnidirectional) {
        angle = RandomFloat(0.0f, UTILS_2PI);
    }
    else {
        angle = emitter._orientation + params.orientation;

        if(!IsFloatEqual(emitter._angle_variation, 0.0f))
            angle += RandomFloat(-emitter._angle_variation, emitter._angle_variation);
    }

    _particles[i].velocity.x = speed * cosf(angle);
    _particles[i].velocity.y = speed * sinf(angle);

    // figure out property variations

    _particles[i].current_size_variation.x  = RandomFloat(-_system_def->keyframes[0].size_variation.x,
            _system_def->keyframes[0].size_variation.x);
    _particles[i].current_size_variation.y  = RandomFloat(-_system_def->keyframes[0].size_variation.y,
            _system_def->keyframes[0].size_variation.y);

    for(int32_t j = 0; j < 4; ++j) {
        _particles[i].current_color_variation[j] = RandomFloat(-_system_def->keyframes[0].color_variation[j],
                _system_def->keyframes[0].color_variation[j]);
    }

    _particles[i].current_rotation_speed_variation = RandomFloat(-_system_def->keyframes[0].rotation_speed_variation,
            _system_def->keyframes[0].rotation_speed_variation);

    if(_system_def->keyframes.size() > 1) {
        // figure out the next keyframe's variations
        _particles[i].next_size_variation.x  = RandomFloat(-_system_def->keyframes[1].size_variation.x,
                                               _system_def->keyframes[1].size_variation.x);
        _particles[i].next_size_variation.y  = RandomFloat(-_system_def->keyframes[1].size_variation.y,
                                               _system_def->keyframes[1].size_variation.y);

        for(int32_t j = 0; j < 4; ++j) {
            _particles[i].next_color_variation[j] = RandomFloat(-_system_def->keyframes[1].color_variation[j],
                                                    _system_def->keyframes[1].color_variation[j]);
        }

        _particles[i].next_rotation_speed_variation = RandomFloat(-_system_def->keyframes[1].rotation_speed_variation,
                _system_def->keyframes[1].rotation_speed_variation);
    } else {
        // if there's only 1 keyframe, then apply the variations now
        for(int32_t j = 0; j < 4; ++j) {
            _particles[i].color[j] += RandomFloat(-_particles[i].current_color_variation[j],
                                                  _particles[i].current_color_variation[j]);
        }

        _particles[i].size.x += RandomFloat(-_particles[i].current_size_variation.x,
                                            _particles[i].current_size_variation.x);
        _particles[i].size.y += RandomFloat(-_particles[i].current_size_variation.y,
                                            _particles[i].current_size_variation.y);

        _particles[i].rotation_speed += RandomFloat(-_particles[i].current_rotation_speed_variation,
                                        _particles[i].current_rotation_speed_variation);
    }

    _particles[i].tangential_acceleration = _system_def->tangential_acceleration;
    if(_system_def->tangential_acceleration_variation != 0.0f)
        _particles[i].tangential_acceleration += RandomFloat(-_system_def->tangential_acceleration_variation,
                _system_def->tangential_acceleration_variation);

    _particles[i].radial_acceleration = _system_def->radial_acceleration;
    if(_system_def->radial_acceleration_variation != 0.0f)
        _particles[i].radial_acceleration += RandomFloat(-_system_def->radial_acceleration_variation,
                                             _system_def->radial_acceleration_variation);

    _particles[i].acceleration.x = _system_def->acceleration.x;
    if(_system_def->acceleration_variation.x != 0.0f)
        _particles[i].acceleration.x += RandomFloat(-_system_def->acceleration_variation.x,
                                        _system_def->acceleration_variation.x);

    _particles[i].acceleration.y = _system_def->acceleration.y;
    if(_system_def->acceleration_variation.y != 0.0f)
        _particles[i].acceleration.y += RandomFloat(-_system_def->acceleration_variation.y,
                                        _system_def->acceleration_variation.y);

    _particles[i].wind_velocity.x = _system_def->wind_velocity.x;
    if(_system_def->wind_velocity_variation.x != 0.0f)
        _particles[i].wind_velocity.x += RandomFloat(-_system_def->wind_velocity_variation.x,
                                         _system_def->wind_velocity_variation.x);

    _particles[i].wind_velocity.y = _system_def->wind_velocity.y;
    if(_system_def->wind_velocity_variation.y != 0.0f)
        _particles[i].wind_velocity.y += RandomFloat(-_system_def->wind_velocity_variation.y,
                                         _system_def->wind_velocity_variation.y);

    _particles[i].damping = _system_def->damping;
    if(_system_def->damping_variation != 0.0f)
        _particles[i].damping += RandomFloat(-_system_def->damping_variation,
                                             _system_def->damping_variation);

    if(_system_def->wave_motion_used) {
        _particles[i].wave_length_coefficient = _system_def->wave_length;
        if(_system_def->wave_length_variation != 0.0f)
            _particles[i].wave_length_coefficient += RandomFloat(-_system_def->wave_length_variation,
                    _system_def->wave_length_variation);

        _particles[i].wave_length_coefficient = UTILS_2PI / _particles[i].wave_length_coefficient;

        _particles[i].wave_half_amplitude = _system_def->wave_amplitude;
        if(_system_def->wave_amplitude != 0.0f)
            _particles[i].wave_half_amplitude += RandomFloat(-_system_def->wave_amplitude_variation,
                                                 _system_def->wave_amplitude_variation);
        _particles[i].wave_half_amplitude *= 0.5f;
    }

    _particles[i].lifetime = _system_def->particle_lifetime
                             + RandomFloat(-_system_def->particle_lifetime_variation,
                                           _system_def->particle_lifetime_variation);
}

}  // namespace vt_mode_manager
