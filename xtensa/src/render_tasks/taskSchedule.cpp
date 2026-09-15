#include "TaskSchedule.h"
#include "../render_core/renderResource.h"
#include "../render_core/log.h"


uint32_t TaskSchedule::TaskCount() const
{
	return taskCount;
}


bool TaskSchedule::HasPendingTasks() const
{
	return ( currentTask != nullptr );
}


void TaskSchedule::Clear()
{
	RenderResource::Cleanup( resourceLifeTime_t::TASK );

	GpuTask* t = tasks;
	while( t != nullptr )
	{
		GpuTask* next = t->GetChild();
		delete t;
		t = next;
	}
	tasks       = nullptr;
	end         = nullptr;
	currentTask = nullptr;
	taskCount   = 0;
}


void TaskSchedule::Link( GpuTask* task )
{
	assert( task );

	GpuTask* chainEnd = task;
	while ( chainEnd->GetChild() != nullptr )
	{
		chainEnd = chainEnd->GetChild();
		++taskCount;
	}

	if ( end == nullptr )
	{
		assert( tasks == nullptr );
		tasks = task;
	}
	else
	{
		end->SetChild( task );
	}

	end = chainEnd;
	++taskCount;
}


void TaskSchedule::FrameBegin()
{
	currentTask = tasks;

	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->FrameBegin();
		t = t->GetChild();
	}
}


void TaskSchedule::FrameEnd()
{
	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->FrameEnd();
		t = t->GetChild();
	}
	assert( currentTask == nullptr );
}


void TaskSchedule::Resize()
{
	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->Resize();
		t = t->GetChild();
	}
}


void TaskSchedule::DrainPending()
{
	while ( currentTask != nullptr )
		currentTask = currentTask->GetChild();
}


void TaskSchedule::IssueNext( CommandList& context )
{
	if ( currentTask->IsEnabled() )
	{
		currentTask->Execute( context );
	}
	currentTask = currentTask->GetChild();
}


void TaskSchedule::AsString() const
{
	LOG_SCOPE_SYSTEM( Render );

	LogMsg( "Schedule" );

	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		LogMsg( "+ <%s>", t->AsString().c_str() );
		t = t->GetChild();
	}
}
