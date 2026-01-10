---
layout: default
title: "Features"
description: "Key features and capabilities of the HardFOC ESP-IDF CI Tools"
nav_order: 1
parent: "📚 Reference"
permalink: /docs/features/
---

# Features

The HardFOC ESP-IDF CI Tools provide comprehensive CI/CD capabilities specifically designed for ESP-IDF development workflows.

## Key Features

{% include feature_list.html features=site.data.features %}

## Workflow Status

Here are some examples of workflow status indicators:

{% include workflow_status.html status="success" message="Build completed successfully" details="All 12 build configurations passed" %}

{% include workflow_status.html status="warning" message="Build completed with warnings" details="Some deprecation warnings found" %}

{% include workflow_status.html status="error" message="Build failed" details="ESP-IDF version compatibility issue" %}

## Alerts and Notifications

{% include alert.html type="info" title="Important Note" content="Make sure to update your ESP-IDF version before running the workflows." %}

{% include alert.html type="warning" content="This feature requires ESP-IDF v5.5 or later." %}

{% include alert.html type="success" content="Your configuration has been validated successfully!" %}

{% include alert.html type="danger" content="Security scan detected potential vulnerabilities in dependencies." %}

## Feature Cards

{% include card.html title="Build Workflow" content="Matrix-based building across multiple ESP-IDF versions and build types with intelligent caching." %}

{% include card.html title="Security Workflow" content="Comprehensive security auditing including dependency scanning, secret detection, and CodeQL analysis." type="featured" %}

## Badges

Here are some example badges:

{% include badge.html text="New" type="success" %}
{% include badge.html text="Deprecated" type="warning" %}
{% include badge.html text="Required" type="danger" %}
{% include badge.html text="Optional" type="info" %}
{% include badge.html text="ESP32" type="primary" %}