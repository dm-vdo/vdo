/*
 * Copyright Red Hat
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "indexer.h"
#include "permassert.h"

/**********************************************************************/
void uds_init_cond(struct cond_var *cond)
{
	int result;

	result = pthread_cond_init(&cond->condition, NULL);
	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_cond_init error");
}

/**********************************************************************/
void uds_signal_cond(struct cond_var *cond)
{
	int result;

	result = pthread_cond_signal(&cond->condition);
	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_cond_signal error");
}

/**********************************************************************/
void uds_broadcast_cond(struct cond_var *cond)
{
	int result;

	result = pthread_cond_broadcast(&cond->condition);
	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_cond_broadcast error");
}

/**********************************************************************/
void uds_wait_cond(struct cond_var *cond, struct mutex *mutex)
{
	int result;

	result = pthread_cond_wait(&cond->condition, &mutex->mutex);
	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_cond_wait error");
}

/**********************************************************************/
void uds_destroy_cond(struct cond_var *cond)
{
	int result;

	result = pthread_cond_destroy(&cond->condition);
	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_cond_destroy error");
}
