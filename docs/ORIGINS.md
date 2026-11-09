# Origin and provenance

I originally developed SkittlesIP with Connor Grimberg as a 2020 engineering-class project. The archived repository contained the README, `imageProcessing_CHT_v6.py`, and four photographs (`s1.jpg`, `s2.jpg`, `s3.jpg`, and `s4.jpg`). The legacy program used Python/OpenCV and displayed its results in a native OpenCV window.

For ChromascopeIP I wrote a new C++ implementation with separate core/CLI/GUI targets. I have not copied the original Python routines into the new source tree. The old implementation can still be inspected through Git history if I need historical context.

I moved the original test photographs unchanged to `samples/skittles/`. I use them as historical regression inputs and will independently confirm rights and permissions before representing those photographs as MIT-licensed or packaging them for broader redistribution. The new source code and documentation are available under the root MIT license; the photographs have separate, presently unspecified terms.

I have also kept third-party source trees outside the repository. I depend on OpenCV 5.1.0, Dear ImGui 1.93.0, GLFW, and OpenGL through system libraries or explicit upstream source acquisition. Before a public binary release I will add a complete distribution notice/license audit.
