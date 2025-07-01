# Augmented-NBBST

Added augmentation on Non-Blocking Binary Search Trees, the trees are based on the Ellen Trees and the augmentation is based on the augmentation technique proposed in this paper by Fatourou, Ruppert (https://arxiv.org/pdf/2405.10506)  
As a preview of what I changed/added:  
After every operation before you exit you propagate your changes up the path you traversed up to the root to update the size of the tree.  
You do that with the double refresh technique.
