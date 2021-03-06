/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"

struct kgsl_pwrscale_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kgsl_device *device, char *buf);
	ssize_t (*store)(struct kgsl_device *device, const char *buf,
			 size_t count);
};

#define to_pwrscale(k) container_of(k, struct kgsl_pwrscale, kobj)
#define pwrscale_to_device(p) container_of(p, struct kgsl_device, pwrscale)
#define to_device(k) container_of(k, struct kgsl_device, pwrscale_kobj)
#define to_pwrscale_attr(a) \
container_of(a, struct kgsl_pwrscale_attribute, attr)
#define to_policy_attr(a) \
container_of(a, struct kgsl_pwrscale_policy_attribute, attr)

#define PWRSCALE_ATTR(_name, _mode, _show, _store) \
struct kgsl_pwrscale_attribute pwrscale_attr_##_name = \
__ATTR(_name, _mode, _show, _store)

/* Master list of available policies */

static struct kgsl_pwrscale_policy *kgsl_pwrscale_policies[] = {
	NULL
};

static ssize_t pwrscale_policy_store(struct kgsl_device *device,
				     const char *buf, size_t count)
{
	int i;
	struct kgsl_pwrscale_policy *policy = NULL;

	/* The special keyword none allows the user to detach all
	   policies */
	if (!strncmp("none", buf, 4)) {
		kgsl_pwrscale_detach_policy(device);
		return count;
	}

	for (i = 0; kgsl_pwrscale_policies[i]; i++) {
		if (!strncmp(kgsl_pwrscale_policies[i]->name, buf,
			     strnlen(kgsl_pwrscale_policies[i]->name,
				PAGE_SIZE))) {
			policy = kgsl_pwrscale_policies[i];
			break;
		}
	}

	if (policy)
		if (kgsl_pwrscale_attach_policy(device, policy))
			return -EIO;

	return count;
}

static ssize_t pwrscale_policy_show(struct kgsl_device *device, char *buf)
{
	int ret;

	if (device->pwrscale.policy)
		ret = snprintf(buf, PAGE_SIZE, "%s\n",
			       device->pwrscale.policy->name);
	else
		ret = snprintf(buf, PAGE_SIZE, "none\n");

	return ret;
}

PWRSCALE_ATTR(policy, 0644, pwrscale_policy_show, pwrscale_policy_store);

static ssize_t pwrscale_avail_policies_show(struct kgsl_device *device,
					    char *buf)
{
	int i, ret = 0;

	for (i = 0; kgsl_pwrscale_policies[i]; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s ",
				kgsl_pwrscale_policies[i]->name);
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "none\n");
	return ret;
}
PWRSCALE_ATTR(avail_policies, 0444, pwrscale_avail_policies_show, NULL);

static struct attribute *pwrscale_attrs[] = {
	&pwrscale_attr_policy.attr,
	&pwrscale_attr_avail_policies.attr,
	NULL
};

static ssize_t policy_sysfs_show(struct kobject *kobj,
				   struct attribute *attr, char *buf)
{
	struct kgsl_pwrscale *pwrscale = to_pwrscale(kobj);
	struct kgsl_device *device = pwrscale_to_device(pwrscale);
	struct kgsl_pwrscale_policy_attribute *pattr = to_policy_attr(attr);
	ssize_t ret;

	if (pattr->show)
		ret = pattr->show(device, pwrscale, buf);
	else
		ret = -EIO;

	return ret;
}

static ssize_t policy_sysfs_store(struct kobject *kobj,
				    struct attribute *attr,
				    const char *buf, size_t count)
{
	struct kgsl_pwrscale *pwrscale = to_pwrscale(kobj);
	struct kgsl_device *device = pwrscale_to_device(pwrscale);
	struct kgsl_pwrscale_policy_attribute *pattr = to_policy_attr(attr);
	ssize_t ret;

	if (pattr->store)
		ret = pattr->store(device, pwrscale, buf, count);
	else
		ret = -EIO;

	return ret;
}

static void policy_sysfs_release(struct kobject *kobj)
{
	struct kgsl_pwrscale *pwrscale = to_pwrscale(kobj);

	complete(&pwrscale->kobj_unregister);
}

static ssize_t pwrscale_sysfs_show(struct kobject *kobj,
				   struct attribute *attr, char *buf)
{
	struct kgsl_device *device = to_device(kobj);
	struct kgsl_pwrscale_attribute *pattr = to_pwrscale_attr(attr);
	ssize_t ret;

	if (pattr->show)
		ret = pattr->show(device, buf);
	else
		ret = -EIO;

	return ret;
}

static ssize_t pwrscale_sysfs_store(struct kobject *kobj,
				    struct attribute *attr,
				    const char *buf, size_t count)
{
	struct kgsl_device *device = to_device(kobj);
	struct kgsl_pwrscale_attribute *pattr = to_pwrscale_attr(attr);
	ssize_t ret;

	if (pattr->store)
		ret = pattr->store(device, buf, count);
	else
		ret = -EIO;

	return ret;
}

static void pwrscale_sysfs_release(struct kobject *kobj)
{
}

static const struct sysfs_ops policy_sysfs_ops = {
	.show = policy_sysfs_show,
	.store = policy_sysfs_store
};

static const struct sysfs_ops pwrscale_sysfs_ops = {
	.show = pwrscale_sysfs_show,
	.store = pwrscale_sysfs_store
};

static struct kobj_type ktype_pwrscale_policy = {
	.sysfs_ops = &policy_sysfs_ops,
	.default_attrs = NULL,
	.release = policy_sysfs_release
};

static struct kobj_type ktype_pwrscale = {
	.sysfs_ops = &pwrscale_sysfs_ops,
	.default_attrs = pwrscale_attrs,
	.release = pwrscale_sysfs_release
};

void kgsl_pwrscale_sleep(struct kgsl_device *device)
{
	if (device->pwrscale.policy && device->pwrscale.policy->sleep)
		device->pwrscale.policy->sleep(device, &device->pwrscale);
}
EXPORT_SYMBOL(kgsl_pwrscale_sleep);

void kgsl_pwrscale_wake(struct kgsl_device *device)
{
	if (device->pwrscale.policy && device->pwrscale.policy->wake)
		device->pwrscale.policy->wake(device, &device->pwrscale);
}
EXPORT_SYMBOL(kgsl_pwrscale_wake);

void kgsl_pwrscale_busy(struct kgsl_device *device)
{
	if (device->pwrscale.policy && device->pwrscale.policy->busy)
		device->pwrscale.policy->busy(device, &device->pwrscale);
}

void kgsl_pwrscale_idle(struct kgsl_device *device)
{
	if (device->pwrscale.policy && device->pwrscale.policy->idle)
		device->pwrscale.policy->idle(device, &device->pwrscale);
}
EXPORT_SYMBOL(kgsl_pwrscale_idle);

int kgsl_pwrscale_policy_add_files(struct kgsl_device *device,
				   struct kgsl_pwrscale *pwrscale,
				   struct attribute_group *attr_group)
{
	int ret;

	init_completion(&pwrscale->kobj_unregister);

	ret = kobject_init_and_add(&pwrscale->kobj,
				   &ktype_pwrscale_policy,
				   &device->pwrscale_kobj,
				   "%s", pwrscale->policy->name);

	if (ret)
		return ret;

	ret = sysfs_create_group(&pwrscale->kobj, attr_group);

	if (ret) {
		kobject_put(&pwrscale->kobj);
		wait_for_completion(&pwrscale->kobj_unregister);
	}

	return ret;
}

void kgsl_pwrscale_policy_remove_files(struct kgsl_device *device,
				       struct kgsl_pwrscale *pwrscale,
				       struct attribute_group *attr_group)
{
	sysfs_remove_group(&pwrscale->kobj, attr_group);
	kobject_put(&pwrscale->kobj);
	wait_for_completion(&pwrscale->kobj_unregister);
}

void kgsl_pwrscale_detach_policy(struct kgsl_device *device)
{
	mutex_lock(&device->mutex);
	if (device->pwrscale.policy != NULL)
		device->pwrscale.policy->close(device, &device->pwrscale);
	device->pwrscale.policy = NULL;
	mutex_unlock(&device->mutex);
}
EXPORT_SYMBOL(kgsl_pwrscale_detach_policy);

int kgsl_pwrscale_attach_policy(struct kgsl_device *device,
				struct kgsl_pwrscale_policy *policy)
{
	int ret;

	if (device->pwrscale.policy != NULL)
		kgsl_pwrscale_detach_policy(device);

	mutex_lock(&device->mutex);
	device->pwrscale.policy = policy;
	ret = device->pwrscale.policy->init(device, &device->pwrscale);
	if (ret)
		device->pwrscale.policy = NULL;
	mutex_unlock(&device->mutex);

	return ret;
}
EXPORT_SYMBOL(kgsl_pwrscale_attach_policy);

int kgsl_pwrscale_init(struct kgsl_device *device)
{
	return kobject_init_and_add(&device->pwrscale_kobj, &ktype_pwrscale,
				    &device->dev->kobj, "pwrscale");
}
EXPORT_SYMBOL(kgsl_pwrscale_init);

void kgsl_pwrscale_close(struct kgsl_device *device)
{
	kobject_put(&device->pwrscale_kobj);
}
EXPORT_SYMBOL(kgsl_pwrscale_close);
