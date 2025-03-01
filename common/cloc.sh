#!/bin/bash

find . -type f -not -path "./.git/*" -not -path "./common/cpp/thirdparty/*" -not -path "*/build*/*" | xargs cloc
