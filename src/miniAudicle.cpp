/*----------------------------------------------------------------------------
miniAudicle
Cocoa GUI to chuck audio programming environment

Copyright (c) 2005-2013 Spencer Salazar.  All rights reserved.
http://chuck.cs.princeton.edu/
http://soundlab.cs.princeton.edu/

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
U.S.A.
-----------------------------------------------------------------------------*/

//-----------------------------------------------------------------------------
// file: miniaudicle.cpp
// desc: Platform independent miniAudicle interface
//
// author: Spencer Salazar (spencer@ccrma.stanford.edu)
// date: Autumn 2005
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#ifdef __PLATFORM_WIN32__
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#include "miniAudicle.h"
#include "chuck_audio.h"
#include "chuck_otf.h"
#include "chuck_errmsg.h"
#include "chuck_audio.h"
#include "chuck.h"
#include "util_string.h"
#include "version.h"
#ifndef __PLATFORM_WIN32__
#include "git-rev.h"
#endif // __PLATFORM_WIN32__

#include "miniAudicle_ui_elements.h"
#include "miniAudicle_import.h"
//using namespace miniAudicle;

#ifndef __MA_IMPORT_MAUI__
#if defined( __MACOSX_CORE__ ) && !defined(__CHIP_MODE__) && !defined(QT_GUI_LIB)
#define __MA_IMPORT_MAUI__
#endif // defined( __MACOSX_CORE__ )
#endif // __MA_IMPORT_MAUI__

// default destination host name
// extern char g_host[256];

t_CKBOOL g_forked = FALSE;

#if defined(__MACOSX_CORE__)
t_CKINT priority = 80;
t_CKINT priority_low = 60;
#elif defined(__PLATFORM_WIN32__)
t_CKINT priority = 0;
t_CKINT priority_low = 0;
#else
t_CKINT priority = 0x7fffffff;
t_CKINT priority_low = 0x7fffffff;
#endif

#if !defined(SAMPLING_RATE_DEFAULT)
    #if defined(__PLATFORM_LINUX__)
        #define SAMPLING_RATE_DEFAULT 48000
    #else
        #define SAMPLING_RATE_DEFAULT 44100
    #endif //
#endif // !defined(SAMPLING_RATE_DEFAULT)


extern const char MA_VERSION[] = ENV_MA_VERSION " (gidora)";
#ifndef __PLATFORM_WIN32__
extern const char MA_ABOUT[] = "version %s\n\
git: " GIT_REVISION "\n\
Copyright (c) Spencer Salazar\n\n\
ChucK: version %s %lu-bit\n\
Copyright (c) Ge Wang and Perry Cook\nhttp://chuck.cs.princeton.edu/";
#else
extern const char MA_ABOUT[] = "version %s\n\
Copyright (c) Spencer Salazar\n\n\
ChucK: version %s %lu-bit\n\
Copyright (c) Ge Wang and Perry Cook\nhttp://chuck.cs.princeton.edu/";
#endif // __PLATFORM_WIN32__

extern const char MA_HELP[] = 
"usage: miniAudicle [options] [files] \n\
options: \n\
 --dacN           use audio output device N\n\
 --adcN           use audio input device N\n\
 --outN/-oN       use N output channels\n\
 --inN/-iN        use N input channels\n\
 --channelsN/-cN  use N input/output channels\n\
 --srateN/-sN     set sample rate to N\n\
 --bufsizeN/-bN   set sample buffer size to N (rounded to nearest power of 2)\n\
 --bufnumN/-nN    use N sample buffers\n\
 --verboseN/-vN   set log level to N (0-10:none-everything)\n\
 --probe          list available audio devices and properties\n\
 --help/--about   print this message\n\
\n\
miniAudicle-%s\n\
http://audicle.cs.princeton.edu/mini/\n\
\n\
ChucK-%s\n\
http://chuck.cs.princeton.edu/\n";


//-----------------------------------------------------------------------------
// name: audio_cb()
// desc: audio callback
//-----------------------------------------------------------------------------
void audio_cb( t_CKSAMPLE * in, t_CKSAMPLE * out, t_CKUINT numFrames,
        t_CKUINT numInChans, t_CKUINT numOutChans, void * data )
{
    ChucK * chuck = (ChucK *) data;
    // call up to ChucK
    chuck->run( in, out, numFrames );
}

//-----------------------------------------------------------------------------
// name: miniAudicle()
// desc: ... 
//-----------------------------------------------------------------------------
miniAudicle::miniAudicle()
{
    vm = NULL;
    m_chuck = NULL;
    compiler = NULL;
    vm_on = FALSE;
    
    class_names = new map< string, t_CKINT >;
    
    next_document_id = 0;
    
    vm_sleep_time = 10000;
    vm_sleep_max = 1;
    
    vm_status_timeouts = 0;
    vm_status_timeouts_max = 20;
    
    vm_options.enable_audio = TRUE;
    vm_options.enable_network = FALSE;
    vm_options.dac = 0;
    vm_options.adc = 0;
    vm_options.srate = SAMPLING_RATE_DEFAULT;
    vm_options.buffer_size = BUFFER_SIZE_DEFAULT;
    vm_options.num_buffers = NUM_BUFFERS_DEFAULT;
    vm_options.num_inputs = 2;
    vm_options.num_outputs = 2;
    vm_options.enable_block = FALSE;
    
    probe();
}

//-----------------------------------------------------------------------------
// name: ~miniAudicle()
// desc: ...
//-----------------------------------------------------------------------------
miniAudicle::~miniAudicle()
{
    if( vm_on )
        stop_vm();

    delete class_names;
    
    // log
    EM_log( CK_LOG_INFO, "miniAudicle instance destroyed..." );
}

//-----------------------------------------------------------------------------
// name: run_code()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::run_code( string & code, string & name, 
                                    vector< string > & args, string & filepath, 
                                    t_CKUINT docid, t_CKUINT & shred_id, 
                                    string & out )
{    
    if( documents.find( docid ) == documents.end() )
        // invalid document id
    {
        out += "internal error at miniAudicle::run_code\n";
        return OTF_MINI_ERROR;
    }

    // compile
    if( !compiler->go( name, NULL, code.c_str(), filepath ) )
    {
        last_result[docid].result = OTF_COMPILE_ERROR;
        last_result[docid].output = string( EM_lasterror() ) + "\n";
        last_result[docid].line = EM_extLineNum;
        
        out += last_result[docid].output;
        return last_result[docid].result;
    }
    
    // allocate the VM message struct
    Chuck_Msg * msg = new Chuck_Msg;

    // fill in the VM message
    msg->code = compiler->output();
        
    msg->code->name = name;
    msg->type = MSG_ADD;
    msg->reply = ( ck_msg_func )1;
    msg->args = new vector< string >( args );
        
    // execute
    vm->queue_msg( msg, 1 );
    
    // check results
    t_OTF_RESULT result = handle_reply( docid, out );
    
    _doc_otf_result otf_result;
    t_CKBOOL gotit = get_last_result( docid, &otf_result );
    if( gotit && result == OTF_SUCCESS )
        shred_id = otf_result.shred_id;
    else
        shred_id = 0;
    
    return result;
}

//-----------------------------------------------------------------------------
// name: replace_code()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::replace_code( string & code, string & name, 
                                        vector< string > & args, string & filepath, 
                                        t_CKUINT docid, t_CKUINT & shred_id, 
                                        string & out )
{    
    if( documents.find( docid ) == documents.end() )
    {
        out += "internal error at miniAudicle::replace_code\n";
        return OTF_MINI_ERROR;
    }
    
    while( documents[docid]->size() && documents[docid]->back() == 0 )
        documents[docid]->pop_back();
    
    if( documents[docid]->size() == 0 )
    {
        out += "no shred to replace\n";
        return OTF_MINI_ERROR;
    }

    if( !compiler->go( name, NULL, code.c_str(), filepath ) )
    {
        last_result[docid].result = OTF_COMPILE_ERROR;
        last_result[docid].output = string( EM_lasterror() ) + "\n";
        last_result[docid].line = EM_extLineNum;
        
        out += last_result[docid].output;
        return last_result[docid].result;
    }
    
    Chuck_Msg * msg = new Chuck_Msg;

    msg->code = compiler->output();
    
    msg->code->name = name;
    msg->type = MSG_REPLACE;
    msg->param = documents[docid]->back();
    msg->reply = ( ck_msg_func )1;
    msg->args = new vector< string >( args );
    
    vm->queue_msg( msg, 1 );
    
    // check results
    t_OTF_RESULT result = handle_reply( docid, out );
    
    _doc_otf_result otf_result;
    t_CKBOOL gotit = get_last_result( docid, &otf_result );
    if( gotit && result == OTF_SUCCESS )
        shred_id = otf_result.shred_id;
    else
        shred_id = 0;
    
    return result;
}

//-----------------------------------------------------------------------------
// name: remove_code()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::remove_code( t_CKUINT docid, t_CKUINT & shred_id, 
                                       string & out )
{
    if( documents.find( docid ) == documents.end() )
    {
        out += "internal error at miniAudicle::remove_code\n";
        return OTF_MINI_ERROR;
    }
    
    while( documents[docid]->size() && documents[docid]->back() == 0 )
        documents[docid]->pop_back();
    
    if( documents[docid]->size() == 0 )
    {
        out += "no shred to remove\n";
        return OTF_MINI_ERROR;
    }
    
    t_CKUINT rm_shred_id = documents[docid]->back();
    
    t_OTF_RESULT result = remove_shred( docid, rm_shred_id, out );
    
    if( result == OTF_SUCCESS )
        shred_id = rm_shred_id;
    else
        shred_id = 0;
    
    return result;
}

//-----------------------------------------------------------------------------
// name: remove_shred()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::remove_shred( t_CKUINT docid, t_CKINT shred_id, 
                                        string & out )
{
    Chuck_Msg * msg = new Chuck_Msg;
    
    msg->type = MSG_REMOVE;
    msg->param = shred_id;
    msg->reply = ( ck_msg_func )1;
    
    vm->queue_msg( msg, 1 );
    
    // check results
    return handle_reply( docid, out );
}

//-----------------------------------------------------------------------------
// name: removeall()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::removeall( t_CKUINT docid, string & out )
{
    Chuck_Msg * msg = new Chuck_Msg;
    
    msg->type = MSG_REMOVEALL;
    msg->reply = ( ck_msg_func )1;
    
    vm->queue_msg( msg, 1 );
    
    // check results
    return handle_reply( docid, out );
}

//-----------------------------------------------------------------------------
// name: removelast()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::removelast( t_CKUINT docid, string & out )
{
    Chuck_Msg * msg = new Chuck_Msg;
    
    msg->type = MSG_REMOVE;
    msg->param = 0xffffffff;
    msg->reply = ( ck_msg_func )1;
    
    vm->queue_msg( msg, 1 );
    
    // 
    return handle_reply( docid, out );
}

//-----------------------------------------------------------------------------
// name: clearvm()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::clearvm( t_CKUINT docid, string & out )
{
    Chuck_Msg * msg = new Chuck_Msg;
    
    msg->type = MSG_CLEARVM;
    msg->reply = ( ck_msg_func )1;
    
    vm->queue_msg( msg, 1 );
    
    // check results
    return handle_reply( docid, out );
}

t_OTF_RESULT miniAudicle::handle_reply( t_CKUINT docid, string & out )
{
    last_result[docid].result = OTF_UNDEFINED;
    otf_docids.push( docid );
    
    int sleep_count = 0;
    
    // process
    while( last_result[docid].result == OTF_UNDEFINED )
    {
        if( !process_reply() )
        {
            if( sleep_count < vm_sleep_max )
            {
                usleep( vm_sleep_time );
                sleep_count++;
            }
            
            else
            {
                return OTF_VM_TIMEOUT;
            }
        }
    }
    
    out += last_result[docid].output;
    
    return last_result[docid].result;
}

Chuck_VM_Status & Chuck_VM_Status_copy( Chuck_VM_Status & a, const Chuck_VM_Status & b )
{
    a = b;
    
    a.list.clear();
    Chuck_VM_Shred_Status * shred;
    for( t_CKUINT i = 0; i < b.list.size(); i++ )
    {
        shred = b.list[i];
        a.list.push_back( new Chuck_VM_Shred_Status( shred->xid, 
                                                     shred->name, 
                                                     shred->start, 
                                                     shred->has_event ) );
    }
    
    return a;
}

//-----------------------------------------------------------------------------
// name: status()
// desc: ...
//-----------------------------------------------------------------------------
t_OTF_RESULT miniAudicle::status( Chuck_VM_Status * status )
{
    if( vm_on != TRUE || vm == NULL )
        return OTF_MINI_ERROR;
    
    t_CKBOOL do_copy_status = TRUE;

    while( process_reply() == TRUE )
        ;
    
    if( do_copy_status && status_bufs[status_bufs_read] && 
        status_bufs[status_bufs_read]->now_system >= status->now_system )
    {
        status->clear();
        Chuck_VM_Status_copy( *status, *( status_bufs[status_bufs_read] ) );
        do_copy_status = FALSE;
    }
    
    for( size_t i = 0; i < num_status_bufs; i++ )
    {
        if( i != status_bufs_read && status_bufs[i] )
        {
            if( do_copy_status )
            {
                status->clear();
                Chuck_VM_Status_copy( *status, *( status_bufs[i] ) );
            }
            
            Chuck_Msg * msg = new Chuck_Msg;
            
            msg->type = MSG_STATUS;
            msg->reply = ( ck_msg_func )1;
            msg->user = status_bufs[i];
            
            status_bufs[i] = NULL;

            vm->queue_msg( msg, 1 );
            
            return OTF_SUCCESS;
        }
    }
    
    //EM_log( CK_LOG_SEVERE, "(miniAudicle): insufficient buffers for status query" );
    
    return OTF_MINI_ERROR;
}

t_CKBOOL miniAudicle::process_reply()
{
    Chuck_Msg * msg;
    
    if( ( msg = vm->get_reply() ) == NULL )
        return FALSE;
    
    if( msg->type == MSG_STATUS )
    {
        vm_status_timeouts = 0;
        size_t i = 0;
        for( i = 0; i < num_status_bufs; i++ )
        {
            if( !status_bufs[i] )
            {
                status_bufs[i] = ( Chuck_VM_Status * ) msg->user;
                status_bufs_read = i;
                break;
            }
        }
        
        if( i == num_status_bufs )
            EM_log( CK_LOG_SEVERE, "(miniAudicle): insufficient buffers for status query, leaking memory" );
        
        delete msg;
        return TRUE;
    }
    
    t_CKUINT shred_id;
    t_CKUINT docid;
    
    docid = otf_docids.front();
    otf_docids.pop();
    
    // if associated document id no longer exists, 
    // then no one cares about this message
    if( !documents.count( docid ) )
    {
        delete msg;
        return TRUE;
    }
    
    switch( msg->type )
    {
        case MSG_ADD:
            shred_id = msg->replyA;
            
            if( shred_id == 0 )
            {
                // if the docid is still valid
                if( documents.count( docid ) )
                {
                    last_result[docid].result = OTF_VM_ERROR;
                    last_result[docid].output = string( EM_lasterror() ) + "\n";
                }
            }
             
            else
            {
                // if the docid is still valid
                if( documents.count( docid ) )
                {
                    documents[docid]->push_back( shred_id );
                    shreds[shred_id].docid = docid;
                    shreds[shred_id].index = documents[docid]->size() - 1;
                    
                    last_result[docid].result = OTF_SUCCESS;
                    last_result[docid].output = string( EM_lasterror() ) + "\n";
                    last_result[docid].shred_id = shred_id;
                }
            }
            
            break;
            
        case MSG_REPLACE:
            shred_id = msg->replyA;
            
            if( shred_id == 0 )
            {
                shreds.erase( documents[docid]->back() );
                documents[docid]->pop_back(); // shred probably no longer exists
                
                last_result[docid].result = OTF_VM_ERROR;
                last_result[docid].output = string( EM_lasterror() ) + "\n";
                last_result[docid].shred_id = shred_id;
            }
            
            else
            {
                // in case the shred id changed, recreate the various map entries
                shreds.erase( documents[docid]->back() );
                documents[docid]->back() = shred_id;
                shreds[shred_id].docid = docid;
                shreds[shred_id].index = documents[docid]->size() - 1;
                
                last_result[docid].result = OTF_SUCCESS;
                last_result[docid].output = string( EM_lasterror() ) + "\n";
            }
            
            break;
            
        case MSG_REMOVE:
            /*
            if( msg->param == 0xffffffff )
                // remove last
            {
                if( msg->replyA == 0 )
                {
                    last_result[docid].result = OTF_VM_ERROR;
                    last_result[docid].output = string( EM_lasterror() ) + "\n";
                }
                
                else
                {
                    shred_id = msg->replyA;
                    // set the corresponding document-shred association to 0, so future calls
                    // to document-based otf functions will ignore that association
                    if( shreds.count( shred_id ) )
                    {
                        if( documents.count( shreds[shred_id].docid ) )
                            documents[shreds[shred_id].docid]->at( shreds[shred_id].index ) = 0;
                        shreds.erase( shred_id );
                    }
                    
                    last_result[docid].result = OTF_SUCCESS;
                    last_result[docid].output = "";
                }
            }
            
            else
            {
                shred_id = msg->param;
                // set the corresponding document-shred association to 0, so future calls
                // to document-based otf functions will ignore that association
                if( shreds.count( shred_id ) )
                {
                    if( documents.count( shreds[shred_id].docid ) )
                        documents[shreds[shred_id].docid]->at( shreds[shred_id].index ) = 0;
                    shreds.erase( shred_id );
                }

                if( msg->replyA == 0 )
                {
                    last_result[docid].result = OTF_VM_ERROR;
                    last_result[docid].output = string( EM_lasterror() ) + "\n";
                }
                
                else
                {                    
                    last_result[docid].result = OTF_SUCCESS;
                    last_result[docid].output = "";
                }
            }
            */
            
            shred_id = msg->replyA;
            // set the corresponding document-shred association to 0, so future calls
            // to document-based otf functions will ignore that association
            if( shreds.count( shred_id ) )
            {
                if( documents.count( shreds[shred_id].docid ) )
                    documents[shreds[shred_id].docid]->at( shreds[shred_id].index ) = 0;
                shreds.erase( shred_id );
            }
                
            if( shred_id == 0 )
            {
                last_result[docid].result = OTF_VM_ERROR;
                last_result[docid].output = string( EM_lasterror() ) + "\n";
            }
                
            else
            {                    
                last_result[docid].result = OTF_SUCCESS;
                last_result[docid].output = "";
            }
                
            break;
            
        case MSG_REMOVEALL:
            if( msg->replyA == 0 )
            {
                last_result[docid].result = OTF_VM_ERROR;
                last_result[docid].output = string( EM_lasterror() ) + "\n";
            }
            
            else
            {
                last_result[docid].result = OTF_SUCCESS;
                last_result[docid].output = "";
            }

            break;
    }
    
    delete msg;
    
    return TRUE;
}

t_CKBOOL miniAudicle::get_last_result( t_CKUINT docid, t_OTF_RESULT * result, 
                                       string * out, int * line_num )
{
    if( last_result.count( docid ) == 0 )
        return FALSE;
    
    if( result )
        *result = last_result[docid].result;
    if( out )
        *out = last_result[docid].output;
    if( line_num )
        *line_num = last_result[docid].line;
    
    return TRUE;
}

t_CKBOOL miniAudicle::get_last_result( t_CKUINT docid, _doc_otf_result * result  )
{
    if( last_result.count( docid ) == 0 )
        return FALSE;
    
    *result = last_result[docid];
    
    return TRUE;
}

t_CKINT miniAudicle::abort_current_shred()
{
    vm->abort_current_shred();
    return 0;
}

t_CKUINT miniAudicle::allocate_document_id()
{    
    next_document_id++;
    
    // ensure that we have a next_document_id that isnt actually in use
    // if there are more than INT_MAX documents this will infinite loop
    while( documents.find( next_document_id ) != documents.end() )
        next_document_id++;
    
    documents[next_document_id] = new vector< t_CKUINT >;
    
    return next_document_id;
}

void miniAudicle::free_document_id( t_CKUINT docid )
{
    if( documents.find( docid ) != documents.end() )
    {
        last_result.erase( docid );
        // delete associated shred records
        // (they still exist in the vm but we don't care)
        vector< t_CKUINT > * doc_shreds = documents[docid];
        vector< t_CKUINT >::size_type i = 0, len = doc_shreds->size();
        for( ; i < len; i++ )
            if( shreds.count( doc_shreds->at( i ) ) )
                shreds.erase( doc_shreds->at( i ) );
        // delete data
        delete documents[docid];
        // remove from map
        documents.erase( docid );
    }
}

//-----------------------------------------------------------------------------
// name: get_log_level()
// desc: ...
//-----------------------------------------------------------------------------
int miniAudicle::get_log_level()
{
    return g_loglevel;
}

//-----------------------------------------------------------------------------
// name: set_log_level()
// desc: ...
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::set_log_level( int l )
{
    EM_setlog( l );
    return FALSE;
}

//-----------------------------------------------------------------------------
// name: start_vm()
// desc: ...
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::start_vm()
{
    char buffer[1024];
    time_t t;
    
    // allocate status buffers
    // allocate alternating buffers for VM status messages
    num_status_bufs = 4;
    status_bufs = new Chuck_VM_Status * [num_status_bufs];
    for( size_t i = 0; i < num_status_bufs; i++ )
        status_bufs[i] = new Chuck_VM_Status;
    status_bufs_read = 0;
    status_bufs_write = 0;
    
    // clear shred management structures
    last_result.clear();
    
    while( !otf_docids.empty() )
        otf_docids.pop();
    
    map< t_CKUINT, vector< t_CKUINT > * >::iterator iter = documents.begin(),
        end = documents.end();
    for( ; iter != end; iter++ )
        iter->second->clear();
    
    shreds.clear();
    
    // clear the class name existence map
    class_names->clear();
    
    time(&t);
    strncpy( buffer, ctime(&t), 24 );
    buffer[24] = '\0';

    // log
    EM_log( CK_LOG_SYSTEM, "-------( %s )-------", buffer );
    EM_log( CK_LOG_SYSTEM, "starting chuck virtual machine..." );
    // push log
    EM_pushlog();
    
    if( m_chuck == NULL )
    {
        // log
        EM_log( CK_LOG_INFO, "allocating VM..." );
        t_CKBOOL enable_audio = vm_options.enable_audio;
        t_CKBOOL vm_halt = FALSE;
        t_CKBOOL force_srate = TRUE;
        t_CKUINT srate = vm_options.srate;
        t_CKUINT buffer_size = vm_options.buffer_size;
        t_CKUINT num_buffers = NUM_BUFFERS_DEFAULT;
        t_CKUINT dac = vm_options.dac;
        t_CKUINT adc = vm_options.adc;
        t_CKBOOL set_priority = FALSE;
        t_CKBOOL block = vm_options.enable_block;
        t_CKUINT output_channels = vm_options.num_outputs;
        t_CKUINT input_channels = vm_options.num_inputs;
        t_CKUINT adaptive_size = 0;
        
        // lets make up some magic numbers...
        vm_sleep_time = vm_options.buffer_size * 1000000 / vm_options.srate;
        vm_sleep_max = 20;
        vm_status_timeouts_max = vm_options.buffer_size / 100;
        
        vm_status_timeouts = 0;

        // check buffer size
        // buffer_size = next_power_2( buffer_size-1 );
        // audio, boost
        if( !set_priority && !block ) priority = priority_low;
        if( !set_priority && !enable_audio ) priority = 0x7fffffff;
        // set priority
        // Chuck_VM::our_priority = priority;
        // set watchdog
#ifdef __MACOSX_CORE__
        g_do_watchdog = TRUE;
        g_watchdog_timeout = .5;
#else
        g_do_watchdog = FALSE;
#endif
        
        std::list<std::string> library_paths = vm_options.library_paths;
        std::list<std::string> named_chugins = vm_options.named_chugins;
        // normalize paths
        for(std::list<std::string>::iterator i = library_paths.begin();
            i != library_paths.end(); i++)
            *i = expand_filepath(*i);
        for(std::list<std::string>::iterator j = named_chugins.begin();
            j != named_chugins.end(); j++)
            *j = expand_filepath(*j);
        
        m_chuck = new ChucK();
        
        m_chuck->setParam(CHUCK_PARAM_SAMPLE_RATE, srate);
        m_chuck->setParam(CHUCK_PARAM_INPUT_CHANNELS, input_channels);
        m_chuck->setParam(CHUCK_PARAM_OUTPUT_CHANNELS, output_channels);
        m_chuck->setParam(CHUCK_PARAM_VM_ADAPTIVE, adaptive_size);
        m_chuck->setParam(CHUCK_PARAM_VM_HALT, vm_halt);
        m_chuck->setParam(CHUCK_PARAM_USER_CHUGINS, named_chugins);
        m_chuck->setParam(CHUCK_PARAM_USER_CHUGIN_DIRECTORIES, library_paths);

        if( !m_chuck->init() )
        {
            fprintf( stderr, "[chuck]: failed to init chuck engine\n" );
            // handle error
            goto error;
        }

        // allocate the vm - needs the type system
        vm = m_chuck->vm();
        
        //--------------------------- AUDIO I/O SETUP ---------------------------------
        
        // push
        EM_pushlog();
        // log
        EM_log( CK_LOG_SYSTEM, "probing '%s' audio subsystem...", enable_audio ? "real-time" : "fake-time" );
        
        ChuckAudio::m_dac_n = dac;
        ChuckAudio::m_adc_n = adc;
        
        // probe / init (this shouldn't start audio yet...
        // moved here 1.3.1.2; to main ge: 1.3.5.3)
        if( !ChuckAudio::initialize( output_channels, input_channels, srate, buffer_size, num_buffers, audio_cb, m_chuck, force_srate ) )
        {
            EM_log( CK_LOG_SYSTEM,
                   "cannot initialize audio device (use --silent/-s for non-realtime)" );
            // pop
            EM_poplog();
            // handle error
            goto error;
        }
        
        // log
        EM_log( CK_LOG_SYSTEM, "real-time audio: %s", enable_audio ? "YES" : "NO" );
        EM_log( CK_LOG_SYSTEM, "mode: %s", block ? "BLOCKING" : "CALLBACK" );
        EM_log( CK_LOG_SYSTEM, "sample rate: %ld", srate );
        EM_log( CK_LOG_SYSTEM, "buffer size: %ld", buffer_size );
        
        if( enable_audio )
        {
            EM_log( CK_LOG_SYSTEM, "num buffers: %ld", num_buffers );
            EM_log( CK_LOG_SYSTEM, "adc: %ld dac: %d", adc, dac );
            EM_log( CK_LOG_SYSTEM, "adaptive block processing: %ld", adaptive_size > 1 ? adaptive_size : 0 );
        }
        EM_log( CK_LOG_SYSTEM, "channels in: %ld out: %ld", input_channels, output_channels );
        
        // pop
        EM_poplog();
        
        // allocate the compiler
        compiler = m_chuck->compiler();
        
#ifdef __MA_IMPORT_MAUI__
        // import api
        init_maui( compiler->env() );
#endif
        for(list<t_CKBOOL (*)(Chuck_Env *)>::iterator i = vm_options.query_funcs.begin(); i != vm_options.query_funcs.end(); i++)
            (*i)( compiler->env() );
        
        if(!ChuckAudio::start())
        {
            EM_log( CK_LOG_SYSTEM, "error starting audio (use --silent/-s for non-realtime)" );
            // pop
            EM_poplog();
            // handle error
            goto error;
        }
        
        // pop log
        EM_poplog();
        EM_log( CK_LOG_SYSTEM, "running audio" );
    }
    
    // set flag
    vm_on = TRUE;
    // pop
    EM_poplog();
    // done
    return vm_on;
    
// in case something goes really wrong above | 1.4.0.2
error:
    // clean up
    stop_vm();
    // pop
    EM_poplog();
    // done
    return FALSE;
}

//-----------------------------------------------------------------------------
// name: stop_vm()
// desc: ...
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::stop_vm()
{
    // delete status bufs
    if( status_bufs )
    {
        for( size_t i = 0; i < num_status_bufs; i++ )
        {
            SAFE_DELETE( status_bufs[i] );
        }
        SAFE_DELETE_ARRAY( status_bufs );
    }
    
    // if it's there
    if( m_chuck )
    {
        // log
        EM_log( CK_LOG_SYSTEM, "stopping chuck virtual machine..." );
        // stop audio, in case it's started
        ChuckAudio::stop();
        // shutdown audio, in case it's initialized
        ChuckAudio::shutdown();
        // clean up chuck global components
        ChucK::globalCleanup();
        // delete and zero out
        SAFE_DELETE(m_chuck);
        // zero out references
        compiler = NULL;
        vm = NULL;
    }

    // set flag to false
    vm_on = FALSE;

    return TRUE;
}

//-----------------------------------------------------------------------------
// name: is_on()
// desc: ...
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::is_on()
{
    return vm_on;
}

//-----------------------------------------------------------------------------
// name: shred_count()
// desc: ...
//-----------------------------------------------------------------------------
t_CKUINT miniAudicle::shred_count()
{
    if( vm_on != TRUE || vm == NULL )
        return 0;
    Chuck_VM_Status status;
    vm->shreduler()->status( &status );
    return (t_CKUINT)status.list.size();
}

void tokenize_string( string & str, vector< string > & tokens)
{
    t_CKINT space = 1;
    t_CKINT end_space = 0;
    t_CKINT dquote = 0;
    t_CKINT i = 0, j = 0, len = str.size();
    
    for( i = 0; i < len; i++ )
    {
        if( isspace( str[i] ) && space )
        {
            j++;
            continue;
        }
        
        if( isspace( str[i] ) && end_space )
        {
            tokens.push_back( string( str, j, i - j ) );
            j = i + 1;
            space = 1;
            end_space = 0;
            continue;
        }
        
        if( str[i] == '"' ) 
        {
            if( !dquote )
            {
                str.erase( i, 1 );
                i--;
                len--;
                space = 0;
                end_space = 0;
                dquote = 1;
                continue;
            }
            
            else if( str[i - 1] == '\\' )
            {
                str.erase( i - 1, 1 );
                len--;
                i--;
                continue;
            }
            
            else
            {
                str.erase( i, 1 );
                i--;
                len--;
                dquote = 0;
                end_space = 1;
                space = 0;
                continue;
            }
        }

        if( !dquote )
        {       
            end_space = 1;
            space = 0;
        }
    }

    if( i > j && end_space )
    {
        tokens.push_back( string( str, j, i - j ) );
    }
}

//-----------------------------------------------------------------------------
// name: highlight_line()
// desc: ...
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::highlight_line( string & line, 
                                      miniAudicle_SyntaxHighlighting * sh )
{
    vector< string > tokens;
    int i, len = line.size();
    
    sh->length = len;
    sh->r = 0x00;
    sh->g = 0x00;
    sh->b = 0x00;
    sh->next = NULL;
    
    i = line.find( "//" );
    if( i != string::npos )
    {
        sh->length = i;
        sh->next = new miniAudicle_SyntaxHighlighting;
        sh = sh->next;
        sh->r = 0xff;
        sh->g = 0x00;
        sh->b = 0x00;
        sh->next = NULL;
        sh->length = len - i;
    }
    
    return TRUE;
}



t_CKBOOL miniAudicle::probe()
{
#ifndef __CHIP_MODE__

    interfaces.clear();
    
    RtAudio * rta = NULL;
    RtAudio::DeviceInfo info;
    
    // allocate RtAudio
    try 
    {
        rta = new RtAudio( );
    }
    catch( RtError & error )
    {
        // problem finding audio devices, most likely
        EM_log( CK_LOG_WARNING, "(RtAudio): %s", error.getMessage().c_str() );
        return FALSE;
    }
    
    // get count    
    int devices = rta->getDeviceCount();
    default_input = devices;
    default_output = devices;
    
    // loop
    for( int i = 0; i < devices; i++ )
    {
        try
        { 
            interfaces.push_back( rta->getDeviceInfo( i ) );
            
            if( interfaces[i].isDefaultInput &&
                interfaces[i].inputChannels &&
                default_input == devices )
                default_input = i;
            
            if( interfaces[i].isDefaultOutput &&
                interfaces[i].outputChannels &&
                default_output == devices )
                default_output = i;
        }
        catch( RtError & error )
        {
            EM_log( CK_LOG_WARNING, "(RtAudio): %s", error.getMessage().c_str() );
            break;
        }
    }
    
    if( default_input == devices )
        // no default input found
        default_input = 0;
    
    if( default_output == devices )
        // no default output found
        default_output = 0;
    
    delete rta;
    
#endif // __CHIP_MODE__
    
    return TRUE;
}


#ifndef __CHIP_MODE__

const vector< RtAudio::DeviceInfo > & miniAudicle::get_interfaces()
{
    return interfaces;
}

#endif // __CHIP_MODE__

//-----------------------------------------------------------------------------
// name: set_num_inputs()
// desc: set number of virtual machine input channels
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::set_num_inputs( t_CKUINT num )
{
#ifndef __CHIP_MODE__
    // sanity check
    int max;
    if( interfaces.size() == 0 )
        max = 2;
    else if( vm_options.adc == 0 )
        max = interfaces[default_input].inputChannels;
    else
        max = interfaces[vm_options.adc - 1].inputChannels;
#else
    int max = 2;
#endif // __CHIP_MODE__
    
    if( num > max )
        vm_options.num_inputs = max;
    else
        vm_options.num_inputs = num;
    
    return TRUE;
}


//-----------------------------------------------------------------------------
// name: get_num_inputs()
// desc: return the number of virtual machine input channels
//-----------------------------------------------------------------------------
t_CKUINT miniAudicle::get_num_inputs()
{
    return vm_options.num_inputs;
}

//-----------------------------------------------------------------------------
// name: set_num_outputs()
// desc: set the number of virtual machine output channels
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::set_num_outputs( t_CKUINT num )
{
#ifndef __CHIP_MODE__
    // sanity check
    int max;
    if( interfaces.size() == 0 )
        max = 2;
    else if( vm_options.dac == 0 )
        max = interfaces[default_output].outputChannels;
    else
        max = interfaces[vm_options.dac - 1].outputChannels;
#else
    int max = 2;
#endif // __CHIP_MODE__

    if( num > max )
        vm_options.num_outputs = max;
    else
        vm_options.num_outputs = num;

    return TRUE;
}

//-----------------------------------------------------------------------------
// name: get_num_outputs()
// desc: return number of virtual machine output channels
//-----------------------------------------------------------------------------
t_CKUINT miniAudicle::get_num_outputs()
{
    return vm_options.num_outputs;
}

//-----------------------------------------------------------------------------
// name: set_enable_audio()
// desc: specify whether or not to enable audio
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::set_enable_audio( t_CKBOOL en )
{
    if( en )
        vm_options.enable_audio = TRUE;
    else
        vm_options.enable_audio = FALSE;
    
    return TRUE;
}

//-----------------------------------------------------------------------------
// name: get_enable_audio()
// desc: determine if audio is enabled
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::get_enable_audio()
{
    return vm_options.enable_audio;
}

//-----------------------------------------------------------------------------
// name: set_enable_network_thread()
// desc: specify whether or not to enable the network command thread
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::set_enable_network_thread( t_CKBOOL en )
{
    if( en )
        vm_options.enable_network = TRUE;
    else
        vm_options.enable_network = FALSE;
    
    return TRUE;
}

//-----------------------------------------------------------------------------
// name: get_enable_audio()
// desc: determine if the network command thread is enabled
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::get_enable_network_thread()
{
    return vm_options.enable_network;
}

t_CKBOOL miniAudicle::set_dac( t_CKUINT dac )
{
#ifndef __CHIP_MODE__
    // sanity check
    if( dac > interfaces.size() )
        return FALSE;
    else
        vm_options.dac = dac;
    
    // set parameters to a reasonable value, if necessary
    set_num_outputs( get_num_outputs() );
#endif // __CHIP_MODE__
    
    return TRUE;
}

t_CKUINT miniAudicle::get_dac()
{
    return vm_options.dac;
}

t_CKBOOL miniAudicle::set_adc( t_CKUINT adc )
{
#ifndef __CHIP_MODE__
    // sanity check
    if( adc > interfaces.size() )
        return FALSE;
    else
        vm_options.adc = adc;

    // set parameters to a reasonable value, if necessary
    set_num_inputs( get_num_inputs() );
#endif // __CHIP_MODE__
    
    return TRUE;
}

t_CKUINT miniAudicle::get_adc()
{
    return vm_options.adc;
}

t_CKBOOL miniAudicle::set_sample_rate( t_CKUINT srate )
{
#ifndef __CHIP_MODE__
    if( interfaces.size() == 0 )
    {
        vm_options.srate = SAMPLING_RATE_DEFAULT;
        return TRUE;
    }
    
    // sanity checks
    // ensure that dac and adc support the given sample rate
    vector< unsigned int > & dac_sample_rates = interfaces[( vm_options.dac ? vm_options.dac - 1 : default_output )].sampleRates;
    vector< unsigned int >::size_type i, len = dac_sample_rates.size();
    for( i = 0; i < len; i++ )
    {
        if( dac_sample_rates[i] == srate )
            break;
    }
    
    if( i == len )
        // the specified sample rate isnt support by the dac
    {
        vm_options.srate = SAMPLING_RATE_DEFAULT; // hope this one works!
        return TRUE;
    }
    
    vector< unsigned int > & adc_sample_rates = interfaces[( vm_options.adc ? vm_options.adc - 1 : default_input )].sampleRates;
    len = adc_sample_rates.size();
    for( i = 0; i < len; i++ )
    {
        if( adc_sample_rates[i] == srate )
            break;
    }
    
    if( i == len )
        // the specified sample rate isnt support by the adc
    {
        vm_options.srate = SAMPLING_RATE_DEFAULT; // hope this one works!
        return TRUE;
    }
#endif // __CHIP_MODE__
    
    vm_options.srate = srate;
    return TRUE;
}

t_CKUINT miniAudicle::get_sample_rate()
{
    return vm_options.srate;
}

t_CKUINT next_power_2( t_CKUINT n )
{
    t_CKUINT nn = n;
    for( ; n &= n-1; nn = n );
    return nn * 2;
}

t_CKBOOL miniAudicle::set_buffer_size( t_CKUINT size )
{
    vm_options.buffer_size = next_power_2( size - 1 );
    return TRUE;
}

t_CKUINT miniAudicle::get_buffer_size()
{
    return vm_options.buffer_size;
}

t_CKBOOL miniAudicle::set_blocking( t_CKBOOL block )
{
    vm_options.enable_block = block;
    return TRUE;
}

t_CKBOOL miniAudicle::get_blocking()
{
    return vm_options.enable_block;
}

t_CKBOOL miniAudicle::set_enable_std_system( t_CKBOOL enable )
{
//    g_enable_system_cmd = enable;
    return TRUE;
}

t_CKBOOL miniAudicle::get_enable_std_system()
{
//    return g_enable_system_cmd;
    return FALSE;
}

t_CKBOOL miniAudicle::set_library_paths( list< string > & paths )
{
    vm_options.library_paths = paths;
    return TRUE;
}

t_CKBOOL miniAudicle::get_library_paths( list< string > & paths )
{
    paths = vm_options.library_paths;
    return TRUE;
}

t_CKBOOL miniAudicle::set_named_chugins( list< string > & chugins )
{
    vm_options.named_chugins = chugins;
    return TRUE;
}

t_CKBOOL miniAudicle::get_named_chugins( list< string > & chugins )
{
    chugins = vm_options.named_chugins;
    return TRUE;
}

t_CKBOOL miniAudicle::add_query_func(t_CKBOOL (*func)(Chuck_Env *))
{
    vm_options.query_funcs.push_back(func);
    return TRUE;
}

//-----------------------------------------------------------------------------
// name: get_new_class_names()
// desc: Pull a list of global class names from the type checker, and determine 
//       which ones are new since the last call to this function.  Uses an 
//       existence map to determine the new classes.  Stores the new names in v.
//-----------------------------------------------------------------------------
t_CKBOOL miniAudicle::get_new_class_names( vector< string > & v )
{
    v.clear();
    
    // get global type names from compiler
    vector< Chuck_Type * > types;
    compiler->env()->global()->get_types( types );
        
    int i, len = types.size();
    for( i = 0; i < len; i++ )
    {
        if( types[i] )
            if( class_names->insert( pair< string, t_CKINT >( types[i]->name, 1 ) ).second )
                v.push_back( types[i]->name );
    }
    
    return TRUE;
}
