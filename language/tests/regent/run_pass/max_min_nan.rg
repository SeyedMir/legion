-- Copyright 2022 Stanford University
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

import "regent"

local nan = 0/0

task main()
  regentlib.assert(max(3.2, nan) == 3.2, "test failed 1")
  regentlib.assert(max(nan, 3.2) == 3.2, "test failed 2")
  regentlib.assert(min(3.2, nan) == 3.2, "test failed 3")
  regentlib.assert(min(nan, 3.2) == 3.2, "test failed 4")
end
regentlib.start(main)
